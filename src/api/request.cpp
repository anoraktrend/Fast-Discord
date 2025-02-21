#include "api/request.h"

#include "api/jsonutils.h"

#include <QJsonDocument>
#include <QDir>
#include <QUrlQuery>
#include <QMimeDatabase>
#include <QtNetwork/QHttpMultiPart>
#include <QtNetwork/QHttpPart>
#include <QtNetwork/QNetworkRequest>

namespace Api {

Requester::Requester(const QString& tokenp)
    : QObject()
{
    // Attributes initialization
    token = tokenp;
    currentRequestsNumber = 0;
    rateLimitEnd = 0; // No rate limit for now
    requestsToCheck = 0;
    stopped = false;

    // Start the request loop
    loop = QThread::create([this](){RequestLoop();});
    loop->start();

    connect(this, SIGNAL(requestEmit(int,QNetworkRequest,QByteArray*,QHttpMultiPart*)), this, SLOT(doRequest(int,QNetworkRequest,QByteArray*,QHttpMultiPart*)));
}

Requester::~Requester()
{
    // Stop the request loop
    stopped = true;
    lock.unlock();
    loop->wait();
    requestWaiter.wakeAll();
}

void const Requester::writeFile()
{
    QFile file(requestQueue.front().outputFile);
    file.open(QIODevice::WriteOnly | QIODevice::Append);
    file.write(reply->readAll());
    file.close();
}

void Requester::readReply()
{
    RequestParameters parameters = requestQueue.dequeue();
    if (parameters.outputFile != "" && parameters.type == GetImage) {
        QDir dir("cache/");
        if (!dir.exists()) dir.mkpath(".");

        QFile file(parameters.outputFile);
        file.open(QIODevice::WriteOnly);
        file.write(reply->readAll());
        file.close();
    }
    QByteArray ba = reply->readAll();

    QVariant statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    if (statusCode.toInt() == 429) { // We are rate limited
        // Set the end of the rate limit
        rateLimitEnd = QJsonDocument::fromJson(ba)["retry_after"].toDouble();
    } else {
        switch (parameters.type) {
            case GetUser:
                {
                    User *user;
                    unmarshal<User>(QJsonDocument::fromJson(ba).object(), &user);
                    parameters.callback(static_cast<void *>(user));
                    break;
                }
            case GetGuilds:
                {
                    QVector<Guild *> guilds = unmarshalMultiple<Guild>(QJsonDocument::fromJson(ba).array());
                    parameters.callback(static_cast<void *>(&guilds));
                    break;
                }
            case GetGuildChannels:
                {
                    QVector<Channel *> channels = unmarshalMultiple<Channel>(QJsonDocument::fromJson(ba).array());
                    parameters.callback(static_cast<void *>(&channels));
                    break;
                }
            case GetPrivateChannels:
                {
                    QVector<Channel *> privateChannels = unmarshalMultiple<Channel>(QJsonDocument::fromJson(ba).array());
                    parameters.callback(static_cast<void *>(&privateChannels));
                    break;
                }
            case GetMessages:
                {
                    QVector<Message *> messages = unmarshalMultiple<Message>(QJsonDocument::fromJson(ba).array());
                    parameters.callback(static_cast<void *>(&messages));
                    break;
                }
            case GetClient:
                {
                    Client *client;
                    unmarshal<Client>(QJsonDocument::fromJson(ba).object(), &client);
                    parameters.callback(static_cast<void *>(client));
                    break;
                }
            case GetClientSettings:
                {
                    ClientSettings *clientSettings;
                    unmarshal<ClientSettings>(QJsonDocument::fromJson(ba).object(), &clientSettings);
                    parameters.callback(static_cast<void *>(clientSettings));
                    break;
                }
            case GetWsUrl:
                {
                    try {
                        QJsonValue jsonUrl = QJsonDocument::fromJson(ba)["url"];
                        QString url = jsonUrl.toString();
                        parameters.callback(static_cast<void *>(&url));
                    } catch(...) {
                        break;
                    }
                break;
                }
            case GetImage:
                {
                    QString imageFileName = parameters.outputFile;
                    lock.lock();
                    if (requestsToCheck == 0) parameters.callback(static_cast<void *>(&imageFileName));
                    lock.unlock();
                    break;
                }
        }
        currentRequestsNumber--;

        lock.lock();
        if (requestsToCheck > 0) requestsToCheck--;
        lock.unlock();
    }
    finishWaiter.wakeOne();
}

void Requester::RequestLoop()
{
    while (!stopped) {
        if (requestQueue.size() > currentRequestsNumber) {
            do {
                RequestParameters parameters = requestQueue.front();
                currentRequestsNumber++;

                if (rateLimitEnd > 0) {
                    // We are currently rate limited
                    QThread::msleep(rateLimitEnd);
                    rateLimitEnd = 0; // Reset rate limit
                }

                QNetworkRequest request(QUrl(parameters.url));

                /* Set the headers */
                if (parameters.type != GetFile)
                    request.setRawHeader(QByteArray("Authorization"), token.toUtf8());
                if (parameters.json)
                    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

                if (!parameters.fileName.isEmpty()) {
                    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
                    QHttpPart part;

                    part.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"payload_json\""));
                    part.setBody(parameters.postDatas.toUtf8());
                    multiPart->append(part);

                    QString fileName = parameters.fileName;
                    part.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"files[0]\"; filename=\"" + fileName.right(fileName.size() - fileName.lastIndexOf("/")) + "\""));
                    part.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(QMimeDatabase().mimeTypeForFile(parameters.fileName).name()));
                    QFile file(parameters.fileName);
                    file.open(QIODevice::ReadOnly);
                    part.setBody(file.readAll());
                    file.close();
                    multiPart->append(part);

                    emit requestEmit(Post, request, nullptr, multiPart);
                } else if (!parameters.customRequest.isEmpty()) {
                    if (parameters.customRequest == "POST") {
                        emit requestEmit(Post, request, new QByteArray(parameters.postDatas.toUtf8()), nullptr);
                    } else if (parameters.customRequest == "PUT") {
                        emit requestEmit(Put, request, new QByteArray(parameters.postDatas.toUtf8()), nullptr);
                    } else if (parameters.customRequest == "DELETE") {
                        emit requestEmit(Delete, request, nullptr, nullptr);
                    }
                } else {
                    emit requestEmit(Get, request, nullptr, nullptr);
                }

                finishWaiter.wait(&lock);
            } while (!requestQueue.empty() && requestQueue.size() > currentRequestsNumber);
        } else {
            requestWaiter.wait(&lock);
        }
    }
}

void Requester::doRequest(int requestType, QNetworkRequest request, QByteArray *postDatas, QHttpMultiPart *multiPart)
{
    switch (requestType) {
        case Get:
            reply = netManager.get(request);
            break;
        case Post:
            if (multiPart != nullptr) {
                reply = netManager.post(request, multiPart);
            } else {
                reply = netManager.post(request, *postDatas);
            }
            break;
        case Put:
            reply = netManager.put(request, *postDatas);
            break;
        case Delete:
            reply = netManager.deleteResource(request);
            break;
    }

    connect(reply, SIGNAL(finished()), this, SLOT(readReply()));
    if (requestQueue.front().type == GetFile)
        connect(reply, SIGNAL(readyRead()), this, SLOT(writeFile()));
}

void Requester::requestApi(const RequestParameters &parameters)
{
    lock.lock();
    requestQueue.enqueue(parameters);
    requestWaiter.wakeOne();
    lock.unlock();
}

void Requester::removeImageRequests()
{
    lock.lock();
    requestsToCheck = currentRequestsNumber;
    for (unsigned int i = requestQueue.size() ; i > 0 ; i--) {
        RequestParameters temp = requestQueue.dequeue();
        if (temp.type != GetImage) requestQueue.enqueue(temp);
    }
    lock.unlock();
}

// Functions that request the API to retrieve data

void const Requester::getImage(Callback callback, const QString& url, const QString& fileName)
{
    requestApi({
        callback,
        url,
        "",
        "",
        "",
        "cache/" + fileName,
        GetImage,
        false});
}

void const Requester::getPrivateChannels(Callback callback)
{
    requestApi({
        callback,
        "https://discord.com/api/v9/users/@me/channels",
        "",        
        "",
        "",
        "",
        GetPrivateChannels,
        false});
}

void const Requester::getGuilds(Callback callback)
{
    requestApi({
        callback,
        "https://discord.com/api/v9/users/@me/guilds",
        "",        
        "",
        "",
        "",
        GetGuilds,
        false});
}

void const Requester::getGuildChannels(Callback callback, const Snowflake& id)
{
    requestApi({
        callback,
        "https://discord.com/api/v9/guilds/" + id + "/channels",
        "",        
        "",
        "",
        "",
        GetGuildChannels,
        false});
}

void const Requester::getMessages(Callback callback, const Snowflake& channelId, const Snowflake& beforeId, unsigned int limit)
{
    limit = limit > 50 ? 50 : limit;

    requestApi({
        callback,
        "https://discord.com/api/v9/channels/" + channelId + "/messages?" + (beforeId != 0 ? "before=" + beforeId + "&" : "" ) + "limit=" + QString::number(limit),
        "",        
        "",
        "",
        "",
        GetMessages,
        false});
}

void const Requester::getClient(Callback callback)
{
    requestApi({
        callback,
        "https://discord.com/api/v9/users/@me",
        "",        
        "",
        "",
        "",
        GetClient,
        false});
}

void const Requester::getClientSettings(Callback callback)
{
    requestApi({
        callback,
        "https://discord.com/api/v9/users/@me/settings",
        "",        
        "",
        "",
        "",
        GetClientSettings,
        false});
}

void const Requester::getUser(Callback callback, const Snowflake& userId)
{
    requestApi({
        callback,
        "https://discord.com/api/v9/users/" + userId,
        "",
        "",
        "",
        "",
        GetUser,
        false});
}

// Functions that request the API to send data

void const Requester::setStatus(const QString& status)
{
    requestApi({
        nullptr,
        "https://discord.com/api/v9/users/@me/settings",
        "status:" + status,
        "PATCH",
        "",
        "",
        SetStatus,
        false});
}

void const Requester::sendTyping(const Snowflake& channelId)
{
    requestApi({
        nullptr,
        "https://discord.com/api/v9/channels/" + channelId + "/typing",
        "",
        "POST",
        "",
        "",
        SendTyping,
        false});
}

void const Requester::sendMessage(const QString& content, const Snowflake& channelId)
{
    requestApi({
        nullptr,
        "https://discord.com/api/v9/channels/" + channelId + "/messages",
        "{\"content\":\"" + content + "\"}",
        "POST",
        "",
        "",
        SendMessage,
        true});
}

void const Requester::sendMessageWithFile(const QString& content, const Snowflake& channelId, const QString& filePath)
{
    requestApi({
        nullptr,
        "https://discord.com/api/v9/channels/" + channelId + "/messages",
        "{\"content\":\"" + content + "\"}",
        "POST",
        filePath,
        "",
        SendMessageWithFile,
        false});
}

void const Requester::deleteMessage(const Snowflake& channelId, const Snowflake& messageId)
{
    requestApi({
        nullptr,
        "https://discord.com/api/v9/channels/" + channelId + "/messages/" + messageId,
        "",
        "DELETE",
        "",
        "",
        DeleteMessage,
        false});
}

void const Requester::pinMessage(const Snowflake& channelId, const Snowflake& messageId)
{
    requestApi({
        nullptr,
        "https://discord.com/api/v9/channels/" + channelId + "/pins/" + messageId,
        "",
        "PUT",
        "",
        "",
        PinMessage,
        false});
}

void const Requester::unpinMessage(const Snowflake& channelId, const Snowflake& messageId)
{
    requestApi({
        nullptr,
        "https://discord.com/api/v9/channels/" + channelId + "/pins/" + messageId,
        "",
        "PUT",
        "",
        "",
        UnpinMessage,
        false});
}

void const Requester::getFile(const QString& url, const QString& filename)
{
    requestApi({
        nullptr,
        url,
        "",
        "",
        "",
        filename,
        GetFile,
        false});
}

} // namespace Api
