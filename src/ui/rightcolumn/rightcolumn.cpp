#include "ui/rightcolumn/rightcolumn.h"

#include "ui/rightcolumn/messagetextinput.h"
#include "ui/rightcolumn/fileuploadbutton.h"
#include "api/jsonutils.h"
#include "api/objects/user.h"

#include <QDateTime>

namespace Ui {

RightColumn::RightColumn(Api::RessourceManager *rmp, const Api::Client *clientp, QWidget *parent)
    : QWidget(parent)
{
    // Attribute initialization
    rm = rmp;
    client = clientp;
    placeholder = true;
    messagesLayout = nullptr;

    // Create some widgets
    header = new ChannelHeader(rm, this);
    messageArea = new MessageArea(rm, this);

    // Create and style the layout
    layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Create a placeholder
    QWidget *placeholder = new QWidget(this);
    placeholder->setStyleSheet("background-color: #36393F;");
    layout->addWidget(placeholder);

    // Style the column
    this->setStyleSheet("background-color: #36393F;");

    QObject::connect(this, &RightColumn::messagesReceived, this, &RightColumn::setMessages);
    QObject::connect(this, &RightColumn::userTypingReceived, this, &RightColumn::setUserTyping);
    QObject::connect(this, &RightColumn::moreMessagesReceived, messageArea, &MessageArea::addMessages);
    QObject::connect(messageArea, &MessageArea::scrollbarHigh, this, &RightColumn::loadMoreMessages);
}

void const RightColumn::setMessages(const QVector<Api::Message *>& messages)
{
    messageArea->clear();
    messageArea->setMessages(messages);
    messagesLayout->insertWidget(1, messageArea);
}

void const RightColumn::setUserTyping(const Api::User *user)
{
    // Get the actual timestamp
    qint64 currentTimestamp = QDateTime::currentSecsSinceEpoch();;

    // Get the user name and the text of the typing label
    QString username = user->username;
    QString text = typingLabel->text();

    // Change the text of the typing label
    if (text != "") {
        // Somemone is already typing, so we change with the new user
        size_t index = (text.indexOf(" is typing") != -1) ? text.indexOf(" is typing") : text.indexOf(" are typing");
        text.resize(text.size() - index);
        text += " " + username + " are typing";
    } else {
        // The user is to only one to type
        text = username + " is typing";
    }

    // Change the text on the label and wait 8 seconds
    typingLabel->setText(text);
    QThread::sleep(typingTimestamp + 8 - currentTimestamp);
    typingLabel->setText("");
}

void RightColumn::clean()
{
    rm->requester->removeImageRequests();

    if (messagesLayout != nullptr) {
        messagesLayout->removeItem(messagesLayout->itemAt(1));
        messagesLayout->removeItem(messagesLayout->itemAt(0));
    }
    messageArea->clear();

    for (unsigned int i = 0 ; i < 10 ; i++) {
        layout->removeItem(layout->itemAt(i));
    }

    // Add a placeholder
    QWidget *placeholderWidget = new QWidget(this);
    placeholderWidget->setStyleSheet("background-color: #36393F;");
    layout->addWidget(placeholderWidget);
    header->close();
}

void RightColumn::openGuildChannel(const QString& channelName, const Api::Snowflake& guildId, const Api::Snowflake& id)
{
    rm->getGuildChannel([&](void *channel){
        openChannel(id, "#" + channelName, reinterpret_cast<Api::Channel *>(channel)->type);
    }, guildId, id);
}

void RightColumn::openPrivateChannel(const QString& channelName, const Api::Snowflake& id)
{
    rm->getPrivateChannel([&](void *channel){
        int type = reinterpret_cast<Api::PrivateChannel *>(channel)->type;
        openChannel(id, (type == Api::DM ? "@" : "") + channelName, type);
    }, id);
}

void RightColumn::openChannel(const Api::Snowflake& channelId, const QString& channelName, int type)
{
    if (type != Api::GuildVoice) {
        rm->requester->removeImageRequests();
        messageArea->clear();
        for (unsigned int i = 0 ; i < 10 ; i++) {
            layout->removeItem(layout->itemAt(i));
        }

        // Create some widgets
        QWidget *messagesContainer = new QWidget(this);
        messagesLayout = new QVBoxLayout(messagesContainer);
        header->close();
        header->openChannel(channelName, type);

        // Change the current opened channel ID
        currentOpenedChannel = channelId;

        // Get the messages of the channel
        if (!rm->hasMessages(channelId)) {
            QWidget *messageAreaPlaceholder = new QWidget(messagesContainer);
            messagesLayout->addWidget(messageAreaPlaceholder);
            rm->getMessages([this](void *messages) {emit messagesReceived(*static_cast<QVector<Api::Message *> *>(messages));}, channelId, 50, false);
        } else {
            rm->getMessages([this](void *messagePtr) {
                QVector<Api::Message *> messages = *reinterpret_cast<QVector<Api::Message *> *>(messagePtr);
                messageArea->setMessages(messages);
            }, channelId, 50, false);
        }

        // Create all the widgets
        QWidget *inputContainer = new QWidget(messagesContainer);
        QHBoxLayout *containerLayout = new QHBoxLayout(inputContainer);
        QWidget *inputBox = new QWidget(inputContainer);
        QHBoxLayout *inputLayout = new QHBoxLayout(inputBox);
        MessageTextInput *textInput = new MessageTextInput(channelName, inputBox);
        FileUploadButton *uploadButton = new FileUploadButton(inputBox);
        
        fileLabel = new QLabel(inputBox);
        QPixmap pix("res/images/svg/archive-icon.svg");
        fileLabel->setPixmap(pix.scaled(18, 24, Qt::KeepAspectRatio));
        fileLabel->setFixedSize(18, 24);
        fileLabel->hide();

        // Add widget to the layout and style it
        inputLayout->addWidget(uploadButton);
        inputLayout->addWidget(textInput);
        inputLayout->addWidget(fileLabel);
        inputLayout->setContentsMargins(16, 0, 16, 0);
        inputLayout->setSpacing(0);

        // Style the input box
        inputBox->setFixedHeight(44);
        inputBox->setStyleSheet("background-color: #40444B;"
                                "height: 44px;"
                                "border-radius: 8px;");

        // Add the input in an other container
        containerLayout->addWidget(inputBox);
        containerLayout->setContentsMargins(16, 0, 16, 0);
        inputLayout->setSpacing(0);

        // Create and style the typing label
        typingLabel = new QLabel(messagesContainer);
        QFont font;
        font.setPixelSize(14);
        font.setFamily("whitney");
        typingLabel->setFont(font);
        typingLabel->setText("");
        typingLabel->setFixedHeight(24);
        typingLabel->setStyleSheet("color: #DCDDDE");

        // Add widgets to the message layout and style it
        messagesLayout->addWidget(header);
        messagesLayout->addWidget(messageArea);
        messagesLayout->addWidget(inputContainer);
        messagesLayout->addWidget(typingLabel);
        messagesLayout->setSpacing(0);
        messagesLayout->setContentsMargins(0, 0, 0, 0);

        // Add a widget to the layout and style it
        layout->addWidget(messagesContainer);
        layout->setContentsMargins(0, 0, 0, 0);

        // Connect signals to slots
        QObject::connect(uploadButton, &FileUploadButton::fileSelected, this, &RightColumn::setUploadFilePath);
        QObject::connect(textInput, &MessageTextInput::returnPressed, this, &RightColumn::sendMessage);
        QObject::connect(textInput, &MessageTextInput::typing, this, &RightColumn::sendTyping);
    }
}

void const RightColumn::addMessage(const Api::Message& message)
{
    // Add the message if it belongs to this channels
    if (message.channelId == currentOpenedChannel) {
        Api::Message *messagep = const_cast<Api::Message *>(&message);
        rm->getMessages([this, messagep](void *messagePtr) {
            QVector<Api::Message *> channelMessages = *reinterpret_cast<QVector<Api::Message *> *>(messagePtr);
            messageArea->addMessage(messagep, channelMessages[1]);
        }, currentOpenedChannel, 1, false);
        emit messageAdded(currentOpenedChannel);
    }
}

void RightColumn::userTyping(const json& data)
{
    // TODO : it's showing typing only for the current opened channel, other typing events are ignored
    Api::Snowflake channelId(data["channel_id"].toVariant().toULongLong());
    if (currentOpenedChannel == channelId) {
        // A user is typing in this channel

        // Get the typing timestamp
        typingTimestamp = data["timestamp"].toInt(-1);

        // Get the user that is typing
        rm->getUser([this](void *user) {emit userTypingReceived(static_cast<Api::User *>(user));}, Api::Snowflake(data["user_id"].toVariant().toULongLong()));
    }
}

void const RightColumn::sendTyping()
{
    // Send typing to the API
    rm->requester->sendTyping(currentOpenedChannel);
}

void const RightColumn::sendMessage(const QString& content)
{
    if (!content.isEmpty() || !filePath.isNull()) {
        // Send a new message to the API and add it to the opened channel
        if (filePath.isNull()) {
            rm->requester->sendMessage(content, currentOpenedChannel);
        } else {
            rm->requester->sendMessageWithFile(content, currentOpenedChannel, filePath);
            filePath.clear();
            fileLabel->hide();
        }

        QString messageTimestamp = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
        QString fakeStr;
        Api::Message *newMessage = new Api::Message {Api::User{QString(client->username), fakeStr, QString(client->avatar), fakeStr, fakeStr, fakeStr, Api::Snowflake(client->id), 0, 0, 0, 0, 2, 2, 2, 2}, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, QVector<Api::Reaction *>(), QVector<Api::Embed *>(), QVector<Api::User *>(), QVector<Api::Attachment *>(), QVector<Api::ChannelMention *>(), QVector<QString>(), QVector<Api::MessageComponent *>(), QVector<Api::StickerItem *>(), QVector<Api::Sticker *>(), QString(content), QString(messageTimestamp), fakeStr, fakeStr, 0, Api::Snowflake(currentOpenedChannel), 0, 0, 0, 0, 0, 0, 0, false, false, false};
        this->addMessage(*newMessage);
    }
}

void const RightColumn::loadMoreMessages()
{
   rm->getMessages([this](void *messages){
       emit moreMessagesReceived(*static_cast<QVector<Api::Message *> *>(messages));
   }, currentOpenedChannel, 50, true);
}

void const RightColumn::setUploadFilePath(const QString& file)
{
    filePath = file;
    fileLabel->show();
}

} // namespace Ui

