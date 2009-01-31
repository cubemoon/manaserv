/*
 *  The Mana World Server
 *  Copyright 2004 The Mana World Development Team
 *
 *  This file is part of The Mana World.
 *
 *  The Mana World  is free software; you can redistribute  it and/or modify it
 *  under the terms of the GNU General  Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or any later version.
 *
 *  The Mana  World is  distributed in  the hope  that it  will be  useful, but
 *  WITHOUT ANY WARRANTY; without even  the implied warranty of MERCHANTABILITY
 *  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 *  more details.
 *
 *  You should  have received a  copy of the  GNU General Public  License along
 *  with The Mana  World; if not, write to the  Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "guildmanager.hpp"
#include "guild.hpp"
#include "defines.h"
#include "account-server/dalstorage.hpp"
#include "chat-server/chatclient.hpp"
#include "chat-server/chathandler.hpp"

GuildManager::GuildManager()
{
    // Load stored guilds from db
    mGuilds = storage->getGuildList();
}

GuildManager::~GuildManager()
{
    for (std::list<Guild*>::iterator itr = mGuilds.begin();
            itr != mGuilds.end(); ++itr)
    {
        delete *itr;
    }
    mGuilds.clear();
}

Guild* GuildManager::createGuild(const std::string &name, int playerId)
{
    Guild *guild = new Guild(name);
    // Add guild to db
    storage->addGuild(guild);

    // Add guild, and add owner
    mGuilds.push_back(guild);
    mOwners.push_back(playerId);

    // put the owner in the guild
    addGuildMember(guild, playerId);

    // Set and save the member rights
    storage->setMemberRights(guild->getId(), playerId, GAL_OWNER);

    guild->setOwner(playerId);

    return guild;
}

void GuildManager::removeGuild(Guild *guild)
{
    if (!guild)
        return;
    storage->removeGuild(guild);
    mOwners.remove(guild->getOwner());
    mGuilds.remove(guild);
    delete guild;
}

void GuildManager::addGuildMember(Guild *guild, int playerId)
{
    if (!guild)
        return;
    storage->addGuildMember(guild->getId(), playerId);
    guild->addMember(playerId);
}

void GuildManager::removeGuildMember(Guild *guild, int playerId)
{
    if (!guild)
        return;

    // remove the user from the guild
    storage->removeGuildMember(guild->getId(), playerId);
    guild->removeMember(playerId);

    // if theres no more members left delete the guild
    if(guild->totalMembers() == 0)
    {
        removeGuild(guild);
    }

    // remove the user from owners list
    std::list<int>::iterator itr = mOwners.begin();
    std::list<int>::iterator itr_end = mOwners.end();
    while (itr != itr_end)
    {
        if ((*itr) == playerId)
            mOwners.remove(playerId);

        ++itr;
    }
}

Guild *GuildManager::findById(short id)
{
    Guild *guild;
    for (std::list<Guild*>::iterator itr = mGuilds.begin(),
            itr_end = mGuilds.end();
            itr != itr_end; ++itr)
    {
        guild = (*itr);
        if (guild->getId() == id)
        {
            return guild;
        }
    }
    return NULL;
}

Guild *GuildManager::findByName(const std::string &name)
{
    Guild *guild;
    for (std::list<Guild*>::iterator itr = mGuilds.begin(),
            itr_end = mGuilds.end();
            itr != itr_end; ++itr)
    {
        guild = (*itr);
        if (guild->getName() == name)
        {
            return guild;
        }
    }
    return NULL;
}

bool GuildManager::doesExist(const std::string &name)
{
    return findByName(name) != NULL;
}

std::vector<Guild*> GuildManager::getGuildsForPlayer(int playerId)
{
    std::vector<Guild*> guildList;

    for (std::list<Guild*>::iterator itr = mGuilds.begin();
            itr != mGuilds.end(); ++itr)
    {
        if((*itr)->checkInGuild(playerId))
        {
            guildList.push_back((*itr));
        }
    }
    return guildList;
}

void GuildManager::disconnectPlayer(ChatClient *player)
{
    std::vector<Guild*> guildList = getGuildsForPlayer(player->characterId);

    for (std::vector<Guild*>::const_iterator itr = guildList.begin();
         itr != guildList.end(); ++itr)
    {
        chatHandler->sendGuildListUpdate((*itr)->getName(),
					 player->characterName,
					 GUILD_EVENT_OFFLINE_PLAYER);
    }
}

int GuildManager::changeMemberLevel(ChatClient *player, Guild *guild,
                                    int playerId, int level)
{
    if (guild->checkInGuild(player->characterId) && guild->checkInGuild(playerId))
    {
        int playerLevel = guild->getUserPermissions(player->characterId);

        if (playerLevel == GAL_OWNER)
        {
            // player can modify anyones permissions
            setUserRights(guild, playerId, level);
            return 0;
        }
    }

    return -1;
}

bool GuildManager::alreadyOwner(int playerId)
{
    std::list<int>::iterator itr = mOwners.begin();
    std::list<int>::iterator itr_end = mOwners.end();

    for (itr; itr != itr_end; ++itr)
    {
        if ((*itr) == playerId)
        {
            return true;
        }
    }

    return false;
}

void GuildManager::setUserRights(Guild *guild, int playerId, int rights)
{
    // Set and save the member rights
    storage->setMemberRights(guild->getId(), playerId, rights);

    // Set with guild
    guild->setUserPermissions(playerId, rights);
}
