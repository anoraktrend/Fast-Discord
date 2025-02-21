#include "ui/rightcolumn/messagewidget.h"

#include "ui/rightcolumn/attachmentfile.h"
#include "api/jsonutils.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QDateTime>
#include <QTextStream>

namespace Ui {

MessageWidget::MessageWidget(Api::RessourceManager *rmp, const Api::Message *message, bool isFirstp, bool separatorBefore, QWidget *parent)
    : QWidget(parent)
{
    // Attributes initialization
    rm = rmp;
    isFirst = isFirstp;

    switch (message->type) {
        case Api::Default:
            defaultMessage(message, separatorBefore);
            break;

        case Api::RecipientAdd:
        case Api::RecipientRemove:
            recipientMessage(message);
            break;

        case Api::CallT:
            callMessage(message);
            break;

        case Api::ChannelNameChange:
            channelNameChangeMessage(message);
            break;

        case Api::ChannelIconChange:
            channelIconChangeMessage(message);
            break;

        case Api::ChannelPinnedMessage:
            channelPinnedMessage(message);
            break;
        
        case Api::GuildMemberJoin:
            guildMemberJoinMessage(message);
            break;

        case Api::ChannelFollowAdd:
            channelFollowAdd(message);
            break;

        case Api::UserPremiumGuildSubscription:
        case Api::UserPremiumGuildSubscriptionTier1:
        case Api::UserPremiumGuildSubscriptionTier2:
        case Api::UserPremiumGuildSubscriptionTier3:
            userPremiumGuildSubscriptionMessage(message);
            break;

        case Api::Reply:
        default:
            defaultMessage(message, separatorBefore);
    }

    // Style
    this->setMinimumHeight(26);
    this->setStyleSheet("padding: 0px;"
                        "margin: 0px;");
}

void const MessageWidget::setAvatar(const QString& avatarFileName)
{
    avatar->setImage(avatarFileName);
}

void const MessageWidget::setReplyAvatar(const QString& avatarFileName)
{
    replyAvatar->setImage(avatarFileName);
}

void MessageWidget::addImage(const QString& filename, int width, int height)
{
    QLabel *imageLabel = new QLabel(this);

    if (width >= height && width > 400) {
        float ratio = width / 400;
        width /= ratio;
        height /= ratio;
    } else if (height > width && height > 400) {
        float ratio = height / 400;
        height /= ratio;
        width /= ratio;
    }
    imageLabel->setFixedSize(width, height);

    QPixmap pixmap(filename);
    imageLabel->setPixmap(pixmap.scaled(width, height, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    dataLayout->addWidget(imageLabel);
}

void MessageWidget::enterEvent(QEvent *)
{
    // Mouse hover : change the stylesheet and show the timestamp label
    setStyleSheet("background-color: #32353B;");
    if (content != nullptr)
        content->setStyleSheet("background-color: #32353B;"
                               "color: #DCDDDE;");
    if (!isFirst && timestampLabel != nullptr) timestampLabel->setText(" " + hoveredTimestamp);
}

void MessageWidget::leaveEvent(QEvent *)
{
    // Reset the stylesheet and hide the timestamp label
    setStyleSheet("background-color: none;");
    if (content != nullptr)
        content->setStyleSheet("background-color: #36393F;"
                               "color: #DCDDDE;");
    if (!isFirst && timestampLabel != nullptr) timestampLabel->setText("");
}

QString const MessageWidget::processTime(const QTime& time)
{
    // Process the time to a nicer format

    // Get the hours and minutes
    int hour = time.hour();
    int minute = time.minute();
    // Process the hours and minutes
    QString hourString = hour > 12 ? QString::number(hour - 12) : QString::number(hour);
    QString minuteString = minute < 10 ? "0" + QString::number(minute) : QString::number(minute);
    // Create the final timestamp format
    QString messageTime = hourString + ":" + minuteString + " ";
    hour / 12 == 1 ? messageTime += "PM" : messageTime += "AM";
    return messageTime;
}

QString const MessageWidget::processTimestamp(const QDateTime& dateTime)
{
    // Get the date of the message and the current date to compare them
    QDate messageDate = dateTime.date();
    QDate currentDate = QDate::currentDate();
    QString date;
    if (messageDate != currentDate) {
        // The message was not sent today
        if (messageDate.year() == currentDate.year() &&
            messageDate.month() == currentDate.month() &&
            messageDate.day() == currentDate.day() - 1) {

            // The message was sent yesterday
            date = "Yesterday at " + processTime(dateTime.time());
        } else {
            // The message is older
            date = (messageDate.month() < 10 ? "0" + QString::number(messageDate.month()) : QString::number(messageDate.month())) + "/" +
                   (messageDate.day() < 10 ? "0" + QString::number(messageDate.day()) : QString::number(messageDate.day())) + "/" +
                   QString::number(messageDate.year());
        }
    } else {
        date = "Today at " + processTime(dateTime.time());
    }

    return date;
}

void MessageWidget::defaultMessage(const Api::Message *message, bool separatorBefore)
{
    // Create the main widgets
    QVBoxLayout *layout = new QVBoxLayout(this);
    QWidget *mainMessage = new QWidget(this);
    QHBoxLayout *mainLayout = new QHBoxLayout(mainMessage);
    QWidget *data = new QWidget(mainMessage);
    dataLayout = new QVBoxLayout(data);
    QWidget *iconContainer = new QWidget(mainMessage);
    QVBoxLayout *iconLayout = new QVBoxLayout(iconContainer);

    QFont font;
    font.setPixelSize(14);
    font.setFamily("whitney");

    // Get the date and time of the message
    QDateTime dateTime = QDateTime::fromString(message->timestamp, Qt::ISODate).toLocalTime();

    if (message->referencedMessage != nullptr && message->referencedMessage->id != 0) {
        Api::Message *ref = message->referencedMessage;
        isFirst = true;

        QWidget *reply = new QWidget(this);
        QHBoxLayout *replyLayout = new QHBoxLayout(reply);

        QLabel *replyIcon = new QLabel(reply);
        replyIcon->setPixmap(QPixmap("res/images/png/reply2.png"));
        replyIcon->setFixedSize(68, 22);
        replyLayout->addWidget(replyIcon);

        // Get the icon of the message
        if (ref->author.avatar.isNull()) {
            // Use an asset if the user doesn't have an icon
            replyLayout->addWidget(new RoundedImage("res/images/png/user-icon-asset0.png", 16, 16, 8, reply));
        } else {
            // Request the avatar
            QString avatarFileName = ref->author.avatar + (ref->author.avatar.indexOf("a_") == 0 ? ".gif" : ".png");
            replyAvatar = new RoundedImage(16, 16, 8, reply);
            replyLayout->addWidget(replyAvatar);
            rm->getImage([this](void *avatarFileName) {this->setReplyAvatar(*reinterpret_cast<QString *>(avatarFileName));}, "https://cdn.discordapp.com/avatars/" + ref->author.id + "/" + avatarFileName, avatarFileName);
        }

        QLabel *username = new QLabel(ref->author.username, reply);
        username->setFont(font);
        username->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
        username->setTextInteractionFlags(Qt::TextSelectableByMouse);
        username->setCursor(QCursor(Qt::PointingHandCursor));

        QLabel *content = new QLabel(ref->content);
        content->setFont(font);
        content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        content->setTextInteractionFlags(Qt::TextSelectableByMouse);
        content->setCursor(QCursor(Qt::PointingHandCursor));
        content->setStyleSheet("color: #B9BBBE;");

        replyLayout->addWidget(username);
        replyLayout->addWidget(content);
        replyLayout->setSpacing(4);

        layout->addWidget(reply);
    }

    if (isFirst) {
        // The message is not grouped to another message

        // Variable creation
        const Api::User& author = message->author;
        QString avatarId = author.avatar;

        // Get the icon of the message
        if (avatarId.isNull()) {
            // Use an asset if the user doesn't have an icon
            avatar = new RoundedImage("res/images/png/user-icon-asset0.png", 40, 40, 20, iconContainer);
            iconLayout->addWidget(avatar);
            iconLayout->setAlignment(avatar, Qt::AlignHCenter);
        } else {
            // Request the avatar
            QString avatarFileName = avatarId + (avatarId.indexOf("a_") == 0 ? ".gif" : ".png");
            avatar = new RoundedImage(40, 40, 20, iconContainer);
            iconLayout->addWidget(avatar);
            iconLayout->setAlignment(avatar, Qt::AlignHCenter);
            rm->getImage([this](void *avatarFileName) {this->setAvatar(*reinterpret_cast<QString *>(avatarFileName));}, "https://cdn.discordapp.com/avatars/" + author.id + "/" + avatarFileName, avatarFileName);
        }

        // Widget to show some infos of the message
        QWidget *messageInfos = new QWidget(data);
        QHBoxLayout *infosLayout = new QHBoxLayout(messageInfos);
        QLabel *name = new QLabel(author.username, messageInfos);
        font.setPixelSize(16);
        name->setFont(font);
        QLabel *date = new QLabel(processTimestamp(dateTime), messageInfos);
        font.setPixelSize(12);
        date->setFont(font);

        // Style the name label
        name->setTextInteractionFlags(Qt::TextSelectableByMouse);
        name->setCursor(QCursor(Qt::IBeamCursor));
        name->setStyleSheet("color: #FFF");

        // Style the date
        date->setTextInteractionFlags(Qt::TextSelectableByMouse);
        date->setStyleSheet("color: #72767D;");

        // Add widgets and style the infos layout
        infosLayout->addWidget(name);
        infosLayout->addWidget(date);
        infosLayout->insertStretch(-1);
        infosLayout->setContentsMargins(0, 0, 0, 0);

        // Add the message infos and style the data layout
        dataLayout->addWidget(messageInfos);
        dataLayout->setSpacing(0);

        // Style the message widget
        this->setContentsMargins(0, separatorBefore ? 0 : 20, 0, 0);
    } else {
        // Add the label that shows the timestamp when the message is hovered
        hoveredTimestamp = processTime(dateTime.time());
        timestampLabel = new QLabel(this);
        font.setPixelSize(12);
        timestampLabel->setFont(font);
        timestampLabel->setFixedHeight(22);
        iconLayout->addWidget(timestampLabel);
        iconLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setContentsMargins(0, 0, 0, 0);
    }

    // Style the icon container
    iconContainer->setFixedWidth(72);
    iconContainer->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
    iconContainer->setStyleSheet("color: #72767D;");
    iconContainer->setLayout(iconLayout);

    // Create and style the content label
    content = nullptr;
    if (!message->content.isNull()) {
        content = new MarkdownLabel(message->content, rm, this);
        dataLayout->addWidget(content);
    }

    // Style the data layout
    dataLayout->setContentsMargins(0, 0, 0, 0);

    if (!message->attachments.empty()) {
        QVector<Api::Attachment *> attachments = message->attachments;
        for (unsigned int i = 0 ; i < attachments.size() ; i++) {
            Api::Attachment *attachment = attachments[i];
            if (attachments[i]->contentType != nullptr
              && attachments[i]->contentType.indexOf("image") != -1
              && attachments[i]->contentType.indexOf("svg") == -1) {
                QString filename = attachments[i]->id +
                        attachments[i]->filename.mid(0, attachments[i]->filename.lastIndexOf('.'));
                rm->getImage([this, attachment](void *filename) {
                    this->addImage(*reinterpret_cast<QString *>(filename), attachment->width, attachment->height);
                }, attachments[i]->proxyUrl, filename);
            } else {
                dataLayout->addWidget(new AttachmentFile(rm->requester, attachment, data));
            }
        }
    }

    // Add widgets to the main layout and style it
    mainLayout->addWidget(iconContainer);
    mainLayout->addWidget(data);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(mainMessage);
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);
}

void MessageWidget::iconMessage(const Api::Message *message, const QString &text, const QString& iconName)
{
    QFont font;
    font.setPixelSize(16);
    font.setFamily("whitney");

    QHBoxLayout *layout = new QHBoxLayout(this);
    QWidget *spacer = new QWidget(this);
    QLabel *icon = new QLabel(this);
    QLabel *textLabel = new QLabel(text, this);
    textLabel->setFont(font);
    QLabel *timestampLabel = new QLabel(processTimestamp(QDateTime::fromString(message->timestamp, Qt::ISODate).toLocalTime()), this);
    timestampLabel->setFont(font);

    spacer->setFixedWidth(28);
    icon->setFixedWidth(44);
    icon->setPixmap(QPixmap("res/images/svg/" + iconName));

    textLabel->setStyleSheet("color: #8E9297;");
    timestampLabel->setStyleSheet("color: #72767D;");
    textLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    textLabel->setCursor(QCursor(Qt::IBeamCursor));
    timestampLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    layout->addWidget(spacer);
    layout->addWidget(icon);
    layout->addWidget(textLabel);
    layout->addWidget(timestampLabel);
    layout->addStretch();

    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);

    this->setMinimumHeight(26);
}

void MessageWidget::recipientMessage(const Api::Message *message)
{
    QString arrowName;
    QString text;

    if (message->type == Api::RecipientAdd) {
        arrowName = "green-right-arrow.svg";
        text = message->mentions[0]->username + " added " +
                message->author.username + " to the group.";
    } else {
        arrowName = "red-left-arrow.svg";
        text = message->mentions[0]->username + " left the group.";
    }

    iconMessage(message, text, arrowName);
}

void MessageWidget::callMessage(const Api::Message *message)
{
    rm->getClient([message, this](void *clientPtr){

    QString phoneName;
    QString text;
    Api::Snowflake clientId = reinterpret_cast<Api::Client *>(clientPtr)->id;
    QDateTime dateTime = QDateTime::fromString(message->timestamp, Qt::ISODate).toLocalTime();

    QVector<Api::Snowflake> participants = message->call->participants;
    bool participated = false;
    for (unsigned int i = 0 ; i < participants.size() ; i++) {
        if (participants[i] == clientId) participated = true;
    }

    if (!participated) {
        text = "You missed a call from " + message->author.username;
    } else {
        text = message->author.username + " started a call";
    }
    QString endedTimestampStr = message->call->endedTimestamp;
    if (!endedTimestampStr.isNull()) {
        phoneName = "gray-phone.svg";
        qint64 duration = QDateTime::fromString(endedTimestampStr, Qt::ISODate)
                .toLocalTime().toSecsSinceEpoch() -
                dateTime.toLocalTime().toSecsSinceEpoch();
        if (duration < 60) {
            text += " that lasted a few seconds";
        } else if (duration < 3600) {
            text += " that lasted ";
            if (duration < 120)
                text += "a minute";
            else
                text += QString::number(duration / 60) + " minutes";
        } else {
            text += " that lasted ";
            if (duration < 7200)
                text += "a hour";
            else
                text += QString::number(duration / 3600) + " hours";
        }
    } else {
        phoneName = "green-phone.svg";
    }
    text += ".";

    if (clientId == message->author.id)
        phoneName = "green-phone.svg";

    iconMessage(message, text, phoneName);

    });
}

void MessageWidget::channelNameChangeMessage(const Api::Message *message)
{
    iconMessage(message, message->author.username + " changed the channel name: " + message->content, "pen.svg");
}

void MessageWidget::channelIconChangeMessage(const Api::Message *message)
{
    iconMessage(message, message->author.username + " changed the channel icon.", "pen.svg");
}

void MessageWidget::channelPinnedMessage(const Api::Message *message)
{
    iconMessage(message, message->author.username + " pinned a message to this channel. See all pinned messages.", "pin.svg");
}

void MessageWidget::userPremiumGuildSubscriptionMessage(const Api::Message *message)
{
    QString text = message->author.username + " just boosted the server.";
    if (message->type == Api::UserPremiumGuildSubscriptionTier1)
        text += "(Tier 1)";
    else if (message->type == Api::UserPremiumGuildSubscriptionTier2)
        text += "(Tier 2)";
    else if (message->type == Api::UserPremiumGuildSubscriptionTier3)
        text += "(Tier 3)";

    iconMessage(message, text, "boost.svg");
}

void MessageWidget::guildMemberJoinMessage(const Api::Message *message)
{
    int randInt = rand() % 39;
    QFile messageFile("res/text/welcome-messages.txt");
    messageFile.open(QIODevice::ReadOnly);
    for (int i = 0 ; i < randInt ; i++) {
        messageFile.readLine();
    }
    QString text = messageFile.readLine();
    int index = text.indexOf("{}");
    while (index != -1){
        text.replace(index, 2, message->author.username);
        index = text.indexOf("{}");
    }

    iconMessage(message, text, "green-right-arrow.svg");
}

void MessageWidget::channelFollowAdd(const Api::Message *message)
{
    iconMessage(message, message->author.username + " has added " + message->content
     + " to this channel. Its most important updates will show up here.", "green-right-arrow.svg");
}

} // namespace Ui
