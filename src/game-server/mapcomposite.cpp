/*
 *  The Mana World Server
 *  Copyright 2006 The Mana World Development Team
 *
 *  This file is part of The Mana World.
 *
 *  The Mana World is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  any later version.
 *
 *  The Mana World is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with The Mana World; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <algorithm>
#include <cassert>

#include "point.h"
#include "game-server/map.hpp"
#include "game-server/mapcomposite.hpp"
#include "game-server/character.hpp"
#include "scripting/script.hpp"
#include "utils/logger.h"

/* TODO: Implement overlapping map zones instead of strict partitioning.
   Purpose: to decrease the number of zone changes, as overlapping allows for
   hysteresis effect and prevents an object from changing zone each server
   tick. It requires storing the zone in the object since it will not be
   uniquely defined any longer. */

/* Pixel-based width and height of the squares used in partitioning the map.
   Squares should be big enough so that a moving object cannot cross several
   ones in one world tick.
   TODO: Tune for decreasing server load. The higher the value, the closer we
   regress to quadratic behavior; the lower the value, the more we waste time
   in dealing with zone changes. */
static int const zoneDiam = 256;

void MapZone::insert(Object *obj)
{
    int type = obj->getType();
    switch (type)
    {
        case OBJECT_CHARACTER:
        {
            if (nbCharacters != nbMovingObjects)
            {
                if (nbMovingObjects != objects.size())
                {
                    objects.push_back(objects[nbMovingObjects]);
                    objects[nbMovingObjects] = objects[nbCharacters];
                }
                else
                {
                    objects.push_back(objects[nbCharacters]);
                }
                objects[nbCharacters] = obj;
                ++nbCharacters;
                ++nbMovingObjects;
                break;
            }
            ++nbCharacters;
        } // no break!
        case OBJECT_MONSTER:
        case OBJECT_NPC:
        {
            if (nbMovingObjects != objects.size())
            {
                objects.push_back(objects[nbMovingObjects]);
                objects[nbMovingObjects] = obj;
                ++nbMovingObjects;
                break;
            }
            ++nbMovingObjects;
        } // no break!
        default:
        {
            objects.push_back(obj);
        }
    }
}

void MapZone::remove(Object *obj)
{
    std::vector< Object * >::iterator i_beg = objects.begin(), i, i_end;
    int type = obj->getType();
    switch (type)
    {
        case OBJECT_CHARACTER:
        {
            i = i_beg;
            i_end = objects.begin() + nbCharacters;
        } break;
        case OBJECT_MONSTER:
        case OBJECT_NPC:
        {
            i = objects.begin() + nbCharacters;
            i_end = objects.begin() + nbMovingObjects;
        } break;
        default:
        {
            i = objects.begin() + nbMovingObjects;
            i_end = objects.end();
        }
    }
    i = std::find(i, i_end, obj);
    assert(i != i_end);
    unsigned pos = i - i_beg;
    if (pos < nbCharacters)
    {
        objects[pos] = objects[nbCharacters - 1];
        pos = nbCharacters - 1;
        --nbCharacters;
    }
    if (pos < nbMovingObjects)
    {
        objects[pos] = objects[nbMovingObjects - 1];
        pos = nbMovingObjects - 1;
        --nbMovingObjects;
    }
    objects[pos] = objects[objects.size() - 1];
    objects.pop_back();
}

static void addZone(MapRegion &r, unsigned z)
{
    MapRegion::iterator i_end = r.end(),
                        i = std::lower_bound(r.begin(), i_end, z);
    if (i == i_end || *i != z)
    {
        r.insert(i, z);
    }
}

ZoneIterator::ZoneIterator(MapRegion const &r, MapContent const *m)
  : region(r), pos(0), map(m)
{
    current = &map->zones[r.empty() ? 0 : r[0]];
}

void ZoneIterator::operator++()
{
    current = NULL;
    if (!region.empty())
    {
        if (++pos != region.size())
        {
            current = &map->zones[region[pos]];
        }
    }
    else
    {
        if (++pos != (unsigned)map->mapWidth * map->mapHeight)
        {
            current = &map->zones[pos];
        }
    }
}

CharacterIterator::CharacterIterator(ZoneIterator const &it)
  : iterator(it), pos(0)
{
    while (iterator && (*iterator)->nbCharacters == 0) ++iterator;
    if (iterator)
    {
        current = static_cast< Character * >((*iterator)->objects[pos]);
    }
}

void CharacterIterator::operator++()
{
    if (++pos == (*iterator)->nbCharacters)
    {
        do ++iterator; while (iterator && (*iterator)->nbCharacters == 0);
        pos = 0;
    }
    if (iterator)
    {
        current = static_cast< Character * >((*iterator)->objects[pos]);
    }
}

MovingObjectIterator::MovingObjectIterator(ZoneIterator const &it)
  : iterator(it), pos(0)
{
    while (iterator && (*iterator)->nbMovingObjects == 0) ++iterator;
    if (iterator)
    {
        current = static_cast< MovingObject * >((*iterator)->objects[pos]);
    }
}

void MovingObjectIterator::operator++()
{
    if (++pos == (*iterator)->nbMovingObjects)
    {
        do ++iterator; while (iterator && (*iterator)->nbMovingObjects == 0);
        pos = 0;
    }
    if (iterator)
    {
        current = static_cast< MovingObject * >((*iterator)->objects[pos]);
    }
}

FixedObjectIterator::FixedObjectIterator(ZoneIterator const &it)
  : iterator(it), pos(0)
{
    while (iterator && (*iterator)->nbMovingObjects == (*iterator)->objects.size()) ++iterator;
    if (iterator)
    {
        pos = (*iterator)->nbMovingObjects;
        current = (*iterator)->objects[pos];
    }
}

void FixedObjectIterator::operator++()
{
    if (++pos == (*iterator)->objects.size())
    {
        do ++iterator; while (iterator && (*iterator)->nbMovingObjects == (*iterator)->objects.size());
        if (iterator)
        {
            pos = (*iterator)->nbMovingObjects;
        }
    }
    if (iterator)
    {
        current = (*iterator)->objects[pos];
    }
}

ObjectIterator::ObjectIterator(ZoneIterator const &it)
  : iterator(it), pos(0)
{
    while (iterator && (*iterator)->objects.empty()) ++iterator;
    if (iterator)
    {
        current = (*iterator)->objects[pos];
    }
}

void ObjectIterator::operator++()
{
    if (++pos == (*iterator)->objects.size())
    {
        do ++iterator; while (iterator && (*iterator)->objects.empty());
        pos = 0;
    }
    if (iterator)
    {
        current = (*iterator)->objects[pos];
    }
}

ObjectBucket::ObjectBucket()
  : free(256), next_object(1)
{
    for (unsigned i = 0; i < 256 / int_bitsize; ++i)
    {
        // An occupied ID is represented by zero in the bitmap.
        bitmap[i] = ~0u;
    }
}

int ObjectBucket::allocate()
{
    // Any free ID in the bucket?
    if (!free)
    {
        return -1;
    }

    /* First check if next_object is a free ID. Directly check inside the
       bitmap, as we have to update it anyway. */
    if (bitmap[next_object / int_bitsize] & (1 << (next_object % int_bitsize)))
    {
        bitmap[next_object / int_bitsize] &= ~(1 << (next_object % int_bitsize));
        int i = next_object;
        next_object = (i + 1) & 255;
        --free;
        return i;
    }

    /* next_object was not free. Check the whole bucket until one ID is found,
       starting from the IDs around next_object. */
    for (unsigned i = 0; i < 256 / int_bitsize; ++i)
    {
        int k = (i + next_object / int_bitsize) & 255;
        // Check int_bitsize IDs at once.
        if (unsigned b = bitmap[k])
        {
            // One of them is free. Find it by looking bit-by-bit.
            int j = 0;
            while (!(b & 1))
            {
                b >>= 1;
                ++j;
            }
            bitmap[k] &= ~(1 << j);
            j += k * int_bitsize;
            next_object = (j + 1) & 255;
            --free;
            return j;
        }
    }

    // No free ID in the bucket.
    return -1;
}

void ObjectBucket::deallocate(int i)
{
    assert(!(bitmap[i / int_bitsize] & (1 << (i % int_bitsize))));
    bitmap[i / int_bitsize] |= 1 << (i % int_bitsize);
    ++free;
}

MapContent::MapContent(Map *map)
  : last_bucket(0), zones(NULL)
{
    buckets[0] = new ObjectBucket;
    for (int i = 1; i < 256; ++i)
    {
        buckets[i] = NULL;
    }
    mapWidth = (map->getWidth() * 32 + zoneDiam - 1) / zoneDiam;
    mapHeight = (map->getHeight() * 32 + zoneDiam - 1) / zoneDiam;
    zones = new MapZone[mapWidth * mapHeight];
}

MapContent::~MapContent()
{
    for (int i = 0; i < 256; ++i)
    {
        delete buckets[i];
    }
    delete[] zones;
}

bool MapContent::allocate(MovingObject *obj)
{
    // First, try allocating from the last used bucket.
    ObjectBucket *b = buckets[last_bucket];
    int i = b->allocate();
    if (i >= 0)
    {
        b->objects[i] = obj;
        obj->setPublicID(last_bucket * 256 + i);
        return true;
    }

    /* If the last used bucket is already full, scan all the buckets for an
       empty place. If none is available, create a new bucket. */
    for (i = 0; i < 256; ++i)
    {
        b = buckets[i];
        if (!b)
        {
            /* Buckets are created in order. If there is nothing at position i,
               there will not be anything in the next positions. So create a
               new bucket. */
            b = new ObjectBucket;
            buckets[i] = b;
        }
        int j = b->allocate();
        if (j >= 0)
        {
            last_bucket = i;
            b->objects[j] = obj;
            obj->setPublicID(last_bucket * 256 + j);
            return true;
        }
    }

    // All the IDs are currently used, fail.
    return false;
}

void MapContent::deallocate(MovingObject *obj)
{
    unsigned short id = obj->getPublicID();
    buckets[id / 256]->deallocate(id % 256);
}

void MapContent::fillRegion(MapRegion &r, Point const &p, int radius) const
{
    int ax = p.x > radius ? (p.x - radius) / zoneDiam : 0,
        ay = p.y > radius ? (p.y - radius) / zoneDiam : 0,
        bx = std::min((p.x + radius) / zoneDiam, mapWidth - 1),
        by = std::min((p.y + radius) / zoneDiam, mapHeight - 1);
    for (int y = ay; y <= by; ++y)
    {
        for (int x = ax; x <= bx; ++x)
        {
            addZone(r, x + y * mapWidth);
        }
    }
}

void MapContent::fillRegion(MapRegion &r, Rectangle const &p) const
{
    int ax = p.x / zoneDiam,
        ay = p.y / zoneDiam,
        bx = std::min((p.x + p.w) / zoneDiam, mapWidth - 1),
        by = std::min((p.y + p.h) / zoneDiam, mapHeight - 1);
    for (int y = ay; y <= by; ++y)
    {
        for (int x = ax; x <= bx; ++x)
        {
            addZone(r, x + y * mapWidth);
        }
    }
}

MapZone& MapContent::getZone(Point const &pos) const
{
    return zones[(pos.x / zoneDiam) + (pos.y / zoneDiam) * mapWidth];
}

MapComposite::MapComposite(int id, std::string const &name):
    mMap(NULL), mContent(NULL), mScript(NULL), mName(name), mID(id)
{
}

MapComposite::~MapComposite()
{
    delete mMap;
    delete mContent;
    delete mScript;
}

ZoneIterator MapComposite::getAroundPointIterator(Point const &p, int radius) const
{
    MapRegion r;
    mContent->fillRegion(r, p, radius);
    return ZoneIterator(r, mContent);
}

ZoneIterator MapComposite::getAroundObjectIterator(Object *obj, int radius) const
{
    MapRegion r;
    mContent->fillRegion(r, obj->getPosition(), radius);
    return ZoneIterator(r, mContent);
}

ZoneIterator MapComposite::getInsideRectangleIterator(Rectangle const &p) const
{
    MapRegion r;
    mContent->fillRegion(r, p);
    return ZoneIterator(r, mContent);
}

ZoneIterator MapComposite::getAroundCharacterIterator(MovingObject *obj, int radius) const
{
    MapRegion r1;
    mContent->fillRegion(r1, obj->getOldPosition(), radius);
    MapRegion r2 = r1;
    for (MapRegion::iterator i = r1.begin(), i_end = r1.end(); i != i_end; ++i)
    {
        /* Fills region with destinations taken around the old position.
           This is necessary to detect two moving objects changing zones at the
           same time and at the border, and going in opposite directions (or
           more simply to detect teleportations, if any). */
        MapRegion &r4 = mContent->zones[*i].destinations;
        if (!r4.empty())
        {
            MapRegion r3;
            r3.reserve(r2.size() + r4.size());
            std::set_union(r2.begin(), r2.end(), r4.begin(), r4.end(),
                           std::back_insert_iterator< MapRegion >(r3));
            r2.swap(r3);
        }
    }
    mContent->fillRegion(r2, obj->getPosition(), radius);
    return ZoneIterator(r2, mContent);
}

bool MapComposite::insert(Thing *ptr)
{
    if (ptr->isVisible())
    {
        if (ptr->canMove() && !mContent->allocate(static_cast< MovingObject * >(ptr)))
        {
            return false;
        }

        Object *obj = static_cast< Object * >(ptr);
        mContent->getZone(obj->getPosition()).insert(obj);
    }

    ptr->setMap(this);
    mContent->things.push_back(ptr);
    return true;
}

void MapComposite::remove(Thing *ptr)
{
    if (ptr->isVisible())
    {
        Object *obj = static_cast< Object * >(ptr);
        mContent->getZone(obj->getPosition()).remove(obj);

        if (ptr->canMove())
        {
            mContent->deallocate(static_cast< MovingObject * >(ptr));
        }
    }

    for (std::vector< Thing * >::iterator i = mContent->things.begin(),
         i_end = mContent->things.end(); i != i_end; ++i)
    {
        if (*i == ptr)
        {
            *i = *(i_end - 1);
            mContent->things.pop_back();
            return;
        }
    }
    assert(false);
}

void MapComposite::setMap(Map *m)
{
    assert(!mMap && m);
    mMap = m;
    mContent = new MapContent(m);
}

void MapComposite::update()
{
    for (int i = 0; i < mContent->mapHeight * mContent->mapWidth; ++i)
    {
        mContent->zones[i].destinations.clear();
    }

    // Cannot use a WholeMap iterator as objects will change zones under its feet.
    for (std::vector< Thing * >::iterator i = mContent->things.begin(),
         i_end = mContent->things.end(); i != i_end; ++i)
    {
        if (!(*i)->canMove())
        {
            continue;
        }
        MovingObject *obj = static_cast< MovingObject * >(*i);

        Point const &pos1 = obj->getOldPosition(),
                    &pos2 = obj->getPosition();

        MapZone &src = mContent->getZone(pos1),
                &dst = mContent->getZone(pos2);
        if (&src != &dst)
        {
            addZone(src.destinations, &dst - mContent->zones);
            src.remove(obj);
            dst.insert(obj);
        }
    }
}

std::vector< Thing * > const &MapComposite::getEverything() const
{
    return mContent->things;
}
