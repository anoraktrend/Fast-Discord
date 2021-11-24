#include "ui/middlecolumn.h"

#include "api/request.h"

#include <fstream>

namespace Ui {

MiddleColumn::MiddleColumn(const Api::Client *client, QWidget *parent)
    : QWidget(parent)
{
    // Create and style the layout
    layout = new QVBoxLayout(this);
    layout->setSpacing(0);

    // Create and style the channel list
    channelList = new QScrollArea(this);
    channelList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Create the user menu at the bottom of the column
    QWidget *userMenu = new QWidget(this);
    QHBoxLayout *userMenuLayout = new QHBoxLayout(userMenu);

    // Get the icon of the actual user
    std::string channelIconFileName;
    if (client->avatar == nullptr) {
        // Use an asset if the user doesn't have an icon
        channelIconFileName = "res/images/png/user-icon-asset.png";
    } else {
        // Request the icon
        channelIconFileName = *client->id + (client->avatar->rfind("a_") == 0 ? ".gif" : ".webp");
        if (!std::ifstream(("cache/" + channelIconFileName).c_str()).good()) {
            Api::Request::requestFile("https://cdn.discordapp.com/avatars/" + *client->id + "/" + *client->avatar, "cache/" + channelIconFileName);
        }
        channelIconFileName = "cache/" + channelIconFileName;
    }

    // Create the widgets of the user menu
    QWidget *userInfos = new QWidget(userMenu);
    QVBoxLayout *userInfosLayout = new QVBoxLayout(userInfos);
    QLabel *name = new QLabel((*client->username).c_str(), userInfos);

    // Style the name label
    name->setFixedSize(84, 18);
    name->setStyleSheet("color: #DCDDDE;");

    // Create and style the discriminator label
    QLabel *discriminator = new QLabel(("#" + *client->discriminator).c_str(), userInfos);
    discriminator->setFixedSize(84, 13);
    discriminator->setStyleSheet("color: #B9BBBE;");

    // Add the widgets and style the user infos layout
    userInfosLayout->addWidget(name);
    userInfosLayout->addWidget(discriminator);
    userInfosLayout->setContentsMargins(0, 10, 0, 10);

    // Add the icon and the infos of the user to the layout and style it
    userMenuLayout->addWidget(new RoundedImage(channelIconFileName, 32, 32, 16, userMenu));
    userMenuLayout->addWidget(userInfos);
    userMenuLayout->setContentsMargins(8, 0, 8, 0);

    // Style the user menu
    userMenu->setFixedHeight(53);
    userMenu->setStyleSheet("background-color: #292B2F");

    // Add the widget and style the main layout
    layout->addWidget(channelList);
    layout->addWidget(userMenu);
    layout->setContentsMargins(2, 0, 0, 0);

    // Display private channels (we are on the home page)
    this->displayPrivateChannels();

    // Style this column
    this->setFixedWidth(240);
    this->setStyleSheet("background-color: #2F3136;"
                        "border: none;");
}

void MiddleColumn::clicChannel(Api::Channel& channel, unsigned int id)
{
    // Reset the stylesheet of the channels except the one that we just clicked
    if (channel.type == Api::DM || channel.type == Api::GroupDM) {
        for (size_t i = 0 ; i < privateChannelWidgets.size() ; i++) {
            if (i != id) {
                privateChannelWidgets[i]->unclicked();
            }
        }
    } else {
        for (size_t i = 0 ; i < guildChannelWidgets.size() ; i++) {
            if (i != id) {
                guildChannelWidgets[i]->unclicked();
            }
        }
    }

    // Emit the signal to open the channel
    emit channelClicked(channel);
}

void MiddleColumn::displayPrivateChannels()
{
    // Request private channels
    privateChannels = Api::Request::getPrivateChannels();

    // Create the widgets
    QWidget *privateChannelList = new QWidget(this);
    QVBoxLayout *privateChannelListLayout = new QVBoxLayout(privateChannelList);

    // Create and display the private channels
    for (unsigned int i = 0 ; i < privateChannels->size() ; i++) {
        PrivateChannel *privateChannel = new PrivateChannel(*(*privateChannels)[i], i);
        privateChannelWidgets.push_back(privateChannel);
        privateChannelListLayout->insertWidget(i, privateChannel);
        QObject::connect(privateChannel, SIGNAL(leftClicked(Api::Channel&, unsigned int)), this, SLOT(clicChannel(Api::Channel&, unsigned int)));
    }
    privateChannelListLayout->insertStretch(-1, 1);

    // Set the channels to the column
    channelList->setWidget(privateChannelList);
}

void MiddleColumn::openGuild(Api::Guild& guild)
{
    // Clear the whannels
    guildChannelWidgets.clear();
    channelList->takeWidget();

    // Create widgets
    QWidget *guildChannelList = new QWidget(this);
    QVBoxLayout *guildChannelListLayout = new QVBoxLayout(guildChannelList);

    // Request the channels of the guild
    std::vector<Api::Channel *> channels = *Api::Request::getGuildChannels(*guild.id);

    // Create the channels widgets

    size_t channelsLen = channels.size();
    unsigned int count; // For the IDs of the channels
    // Loop to find channel that are not in a category
    for (size_t i = 0 ; i < channelsLen ; i++) {
        if ((*channels[i]).type != Api::GuildCategory && (*channels[i]).parentId == nullptr) {
            // Create and add the channel widget to the list
            GuildChannelWidget *channelWidget = new GuildChannelWidget(*channels[i], count, guildChannelList);
            guildChannelListLayout->addWidget(channelWidget);
            guildChannelWidgets.push_back(channelWidget);
            count++;
        }
    }
    // Loop through all channels to create widgets
    for (size_t i = 0 ; i < channelsLen ; i++) {
        if ((*channels[i]).type == Api::GuildCategory) {
            // Create the category channel channel widget
            GuildChannelWidget *channelWidget = new GuildChannelWidget(*channels[i], count, guildChannelList);
            guildChannelListLayout->addWidget(channelWidget);
            guildChannelWidgets.push_back(channelWidget);
            count++;
            // Loop another time to find channels belonging to this category
            for (size_t j = 0 ; j < channelsLen ; j++) {
                if ((*channels[j]).parentId == nullptr) continue;
                    // Category or 'orphan' channel
                if (*(*channels[j]).parentId == *(*channels[i]).id) {
                    // This channel belongs to the category
                    // Create and add the channel widget
                    GuildChannelWidget *channelWidget = new GuildChannelWidget(*channels[j], count, guildChannelList);
                    guildChannelWidgets.push_back(channelWidget);
                    guildChannelListLayout->addWidget(channelWidget);
                    count++;

                    // Connect the clicked signal to open the channel
                    QObject::connect(channelWidget, SIGNAL(leftClicked(Api::Channel&, unsigned int)), this, SLOT(clicChannel(Api::Channel&, unsigned int)));
                }
            }
        }
    }
    guildChannelListLayout->insertStretch(-1, 1);

    // Style the channel list
    channelList->setWidget(guildChannelList);
    channelList->setStyleSheet("* {background-color: #2f3136; border: none;}"
                                     "QScrollBar::handle:vertical {border: none; border-radius: 2px; background-color: #202225;}"
                                     "QScrollBar:vertical {border: none; background-color: #2F3136; border-radius: 8px; width: 3px;}"
                                     "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {border:none; background: none; height: 0;}");
}

} // namespace Ui
