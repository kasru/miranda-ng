#include "stdafx.h"

static int CompareServices(const CCloudService *p1, const CCloudService *p2)
{
	return mir_strcmp(p1->GetModule(), p2->GetModule());
}

LIST<CCloudService> Services(10, CompareServices);

void InitServices()
{
	Services.insert(new CDropboxService(hNetlibConnection));
	Services.insert(new CGDriveService(hNetlibConnection));
	Services.insert(new COneDriveService(hNetlibConnection));
	Services.insert(new CYandexService(hNetlibConnection));

	PROTOCOLDESCRIPTOR pd = { sizeof(pd) };

	size_t count = Services.getCount();
	for (size_t i = 0; i < count; i++) {
		CCloudService *service = Services[i];

		if (!db_get_b(NULL, service->GetModule(), "IsEnable", TRUE))
			continue;

		CMStringA moduleName(CMStringDataFormat::FORMAT, "%s/%s", MODULE, service->GetModule());
		pd.type = PROTOTYPE_VIRTUAL;
		pd.szName = moduleName.GetBuffer();
		Proto_RegisterModule(&pd);

		CMStringA serviceName(CMStringDataFormat::FORMAT, "%s%s", moduleName, PSS_FILE);
		CreateServiceFunctionObj(serviceName, ProtoSendFile, service);

		moduleName = CMStringA(CMStringDataFormat::FORMAT, "%s/%s/Interceptor", MODULE, service->GetModule());
		pd.szName = moduleName.GetBuffer();
		pd.type = PROTOTYPE_FILTER;
		Proto_RegisterModule(&pd);

		serviceName = CMStringA(CMStringDataFormat::FORMAT, "%s%s", moduleName, PSS_FILE);
		CreateServiceFunctionObj(serviceName, ProtoSendFileInterceptor, service);
	}
}

CCloudService::CCloudService(HNETLIBUSER hConnection)
	: hConnection(hConnection)
{
}

void CCloudService::OpenUploadDialog(MCONTACT hContact)
{
	char *proto = GetContactProto(hContact);
	if (!mir_strcmpi(proto, META_PROTO))
		hContact = CallService(MS_MC_GETMOSTONLINECONTACT, hContact);

	auto it = InterceptedContacts.find(hContact);
	if (it == InterceptedContacts.end())
	{
		HWND hwnd = (HWND)CallService(MS_FILE_SENDFILE, hContact, 0);
		InterceptedContacts[hContact] = hwnd;
	}
	else
		SetActiveWindow(it->second);
}

void CCloudService::SendToContact(MCONTACT hContact, const wchar_t *data)
{
	const char *szProto = GetContactProto(hContact);
	if (db_get_b(hContact, szProto, "ChatRoom", 0) == TRUE) {
		ptrW tszChatRoom(db_get_wsa(hContact, szProto, "ChatRoomID"));
		Chat_SendUserMessage(szProto, tszChatRoom, data);
		return;
	}

	char *message = mir_utf8encodeW(data);
	if (ProtoChainSend(hContact, PSS_MESSAGE, 0, (LPARAM)message) != ACKRESULT_FAILED)
		AddEventToDb(hContact, EVENTTYPE_MESSAGE, DBEF_UTF | DBEF_SENT, (DWORD)mir_strlen(message), (PBYTE)message);
}

void CCloudService::PasteToInputArea(MCONTACT hContact, const wchar_t *data)
{
	CallService(MS_MSG_SENDMESSAGEW, hContact, (LPARAM)data);
}

void CCloudService::PasteToClipboard(const wchar_t *data)
{
	if (OpenClipboard(NULL)) {
		EmptyClipboard();

		size_t size = sizeof(wchar_t) * (mir_wstrlen(data) + 1);
		HGLOBAL hClipboardData = GlobalAlloc(NULL, size);
		if (hClipboardData) {
			wchar_t *pchData = (wchar_t*)GlobalLock(hClipboardData);
			if (pchData) {
				memcpy(pchData, (wchar_t*)data, size);
				GlobalUnlock(hClipboardData);
				SetClipboardData(CF_UNICODETEXT, hClipboardData);
			}
		}
		CloseClipboard();
	}
}

void CCloudService::Report(MCONTACT hContact, const wchar_t *data)
{
	if (db_get_b(NULL, MODULE, "UrlAutoSend", 1))
		SendToContact(hContact, data);

	if (db_get_b(NULL, MODULE, "UrlPasteToMessageInputArea", 0))
		PasteToInputArea(hContact, data);

	if (db_get_b(NULL, MODULE, "UrlCopyToClipboard", 0))
		PasteToClipboard(data);
}

char* CCloudService::PreparePath(const char *oldPath, char *newPath)
{
	if (oldPath == NULL)
		mir_strcpy(newPath, "");
	else if (*oldPath != '/')
	{
		CMStringA result("/");
		result.Append(oldPath);
		result.Replace("\\", "/");
		mir_strcpy(newPath, result);
	}
	else
		mir_strcpy(newPath, oldPath);
	return newPath;
}

char* CCloudService::PreparePath(const wchar_t *oldPath, char *newPath)
{
	return PreparePath(ptrA(mir_utf8encodeW(oldPath)), newPath);
}

char* CCloudService::HttpStatusToError(int status)
{
	switch (status) {
	case HTTP_CODE_OK:
		return "OK";
	case HTTP_CODE_BAD_REQUEST:
		return "Bad input parameter. Error message should indicate which one and why";
	case HTTP_CODE_UNAUTHORIZED:
		return "Bad or expired token. This can happen if the user or Dropbox revoked or expired an access token. To fix, you should re-authenticate the user";
	case HTTP_CODE_FORBIDDEN:
		return "Bad OAuth request (wrong consumer key, bad nonce, expired timestamp...). Unfortunately, re-authenticating the user won't help here";
	case HTTP_CODE_NOT_FOUND:
		return "File or folder not found at the specified path";
	case HTTP_CODE_METHOD_NOT_ALLOWED:
		return "Request method not expected (generally should be GET or POST)";
	case HTTP_CODE_TOO_MANY_REQUESTS:
		return "Your app is making too many requests and is being rate limited. 429s can trigger on a per-app or per-user basis";
	case HTTP_CODE_SERVICE_UNAVAILABLE:
		return "If the response includes the Retry-After header, this means your OAuth 1.0 app is being rate limited. Otherwise, this indicates a transient server error, and your app should retry its request.";
	}

	return "Unknown error";
}

void CCloudService::HandleHttpError(NETLIBHTTPREQUEST *response)
{
	if (response == NULL)
		throw Exception(HttpStatusToError());

	if (!HTTP_CODE_SUCCESS(response->resultCode)) {
		if (response->dataLength)
			throw Exception(response->pData);
		throw Exception(HttpStatusToError(response->resultCode));
	}
}

JSONNode CCloudService::GetJsonResponse(NETLIBHTTPREQUEST *response)
{
	HandleHttpError(response);

	JSONNode root = JSONNode::parse(response->pData);
	if (root.isnull())
		throw Exception(HttpStatusToError());

	HandleJsonError(root);

	return root;
}