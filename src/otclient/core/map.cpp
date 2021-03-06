/*
 * Copyright (c) 2010-2012 OTClient <https://github.com/edubart/otclient>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "map.h"
#include "game.h"
#include "localplayer.h"
#include "tile.h"
#include "item.h"
#include "missile.h"
#include "statictext.h"

#include <framework/core/eventdispatcher.h>
#include "mapview.h"
#include <framework/core/resourcemanager.h>
#include <framework/core/filestream.h>
#include <framework/core/binarytree.h>

Map g_map;

void Map::terminate()
{
    clean();
}

void Map::addMapView(const MapViewPtr& mapView)
{
    m_mapViews.push_back(mapView);
}

void Map::removeMapView(const MapViewPtr& mapView)
{
    auto it = std::find(m_mapViews.begin(), m_mapViews.end(), mapView);
    if(it != m_mapViews.end())
        m_mapViews.erase(it);
}

void Map::notificateTileUpdateToMapViews(const Position& pos)
{
    for(const MapViewPtr& mapView : m_mapViews)
        mapView->onTileUpdate(pos);
}

bool Map::loadOtbm(const std::string& fileName)
{
    FileStreamPtr fin = g_resources.openFile(fileName);
    if (!fin) {
        g_logger.error(stdext::format("Unable to load map '%s'", fileName));
        return false;
    }

    if (!g_things.isOtbLoaded()/* || !g_things.isXmlLoaded()*/) {
        g_logger.error("OTB and XML are not loaded yet to load a map.");
        return false;
    }

    assert(fin->getU32() == 0);
    BinaryTreePtr root = fin->getBinaryTree();
    root->skip(1); // Not sure, what exactly that is... perhaps node type?
    uint32 headerVersion = root->getU32();
    dump << headerVersion;
    if (!headerVersion || headerVersion > 3) {
        g_logger.error("Unknown OTBM version detected.");
        return false;
    }

    root->skip(1); // ??
    uint32 headerMajorItems = root->getU8();
    dump << headerMajorItems;
    dump << "major: " << g_things.getOtbMajorVersion();
    if (headerMajorItems < 3) {
        g_logger.error("This map needs to be upgraded.");
        return false;
    }

    if (headerMajorItems > g_things.getOtbMajorVersion()) {
        g_logger.error("This map was saved with different OTB version.");
        return false;
    }

    root->skip(1); /// ??
    uint32 headerMinorItems =  root->getU8();
    dump << headerMinorItems;
    dump << "minor: " << g_things.getOtbMinorVersion();
    if (headerMinorItems > g_things.getOtbMinorVersion())
        g_logger.warning("This map needs an updated OTB.");

    BinaryTreePtr node = root->getChildren()[0];
    if (node->getU8() != OTBM_MAP_DATA) {
        g_logger.error("Could not read data node.");
        return false;
    }

    std::string tmp;
    uint8 attribute;
    while (node->canRead()) {
        attribute = node->getU8();
        tmp = node->getString();
        switch (attribute) {
        case OTBM_ATTR_DESCRIPTION:
            if (!m_description.empty())
                m_description += "\n" + tmp;
            else
                m_description = tmp;
            break;
        case OTBM_ATTR_SPAWN_FILE:
            m_spawnFile = fileName.substr(0, fileName.rfind('/') + 1) + tmp;
            break;
        case OTBM_ATTR_HOUSE_FILE:
            m_houseFile = fileName.substr(0, fileName.rfind('/') + 1) + tmp;
            break;
        default:
            g_logger.error(stdext::format("Invalid attribute '%c'", attribute));
            break;
        }
    }

    dump << m_description;
    dump << m_houseFile;
    dump << m_spawnFile;

    for (const BinaryTreePtr &nodeMapData : node->getChildren()) {
        uint8 mapDataType = nodeMapData->getU8();
        dump << "mapData: " << (int)mapDataType;
        if (mapDataType == OTBM_TILE_AREA) {
            dump << "Tile: reading pos...";
            uint16 baseX = nodeMapData->getU16(), baseY = nodeMapData->getU16();
            uint8 pz = nodeMapData->getU8();

            for (const BinaryTreePtr &nodeTile : nodeMapData->getChildren()) {
                uint8 type = nodeTile->getU8();
                if (type == OTBM_TILE || type == OTBM_HOUSETILE) {
                    TilePtr tile = 0;
                    ItemPtr ground = 0;
                    uint32 flags = 0;

                    uint16 px = baseX + nodeTile->getU16(), py = baseY + nodeTile->getU16();
                    Position pos(px, py, pz);

                    // TODO: Houses.
                    if (type ==  OTBM_HOUSETILE) {
                        uint32 hId = nodeTile->getU32();

                        tile = createTile(pos);
                        // TODO: add it to house.
                    }

                    uint8 tileAttr;
                    while (nodeTile->canRead()) {
                        tileAttr = nodeTile->getU8();
                        switch (tileAttr) {
                        case OTBM_ATTR_TILE_FLAGS: {
                            uint32 _flags = nodeTile->getU32();

                            if ((_flags & TILESTATE_PROTECTIONZONE) == TILESTATE_PROTECTIONZONE)
                                flags |= TILESTATE_PROTECTIONZONE;
                            else if ((_flags & TILESTATE_OPTIONALZONE) == TILESTATE_OPTIONALZONE)
                                flags |= TILESTATE_OPTIONALZONE;
                            else if ((_flags & TILESTATE_HPPARDCOREZONE) == TILESTATE_HPPARDCOREZONE)
                                flags |= TILESTATE_HPPARDCOREZONE;

                            if ((_flags & TILESTATE_NOLOGOUT) == TILESTATE_NOLOGOUT)
                                flags |= TILESTATE_NOLOGOUT;

                            if ((_flags & TILESTATE_REFRESH) == TILESTATE_REFRESH)
                                flags |= TILESTATE_REFRESH;
                            break;
                        } case OTBM_ATTR_ITEM: {
                            ItemPtr item = Item::createFromOtb(nodeTile->getU16());
                            if (!item) {
                                g_logger.error(stdext::format("failed to create new item at tile pos %d, %d, %d", px, py, pz));
                                return false;
                            }

                            if (tile) {
                                tile->addThing(item);
                            } else if (item->isGround()) {
                                ground = item;
                            } else {
                                tile = createTile(pos);
                                tile->addThing(ground);
                                tile->addThing(item);
                            }
                        } default: {
                            g_logger.error(stdext::format("invalid tile attribute at pos %d, %d, %d", px, py, pz));
                            return false;
                        }
                        }
                    }

                    for (const BinaryTreePtr &nodeItem : nodeTile->getChildren()) {
                        if (nodeItem->getU8() == OTBM_ITEM) {
                            ItemPtr item = Item::createFromOtb(nodeItem->getU16());
                            if (!item) {
                                g_logger.error(stdext::format("failed to create new item at pos %d, %d, %d", px, py, pz));
                                return false;
                            }

                            if (item->unserializeItemNode(nodeItem)) {
                                if (/* house  && */item->isMoveable()) {
                                    g_logger.warning(stdext::format("Moveable item found in house: %d at pos %d %d %d", item->getId(),
                                                                    px, py, pz));
                                    item = nullptr;
                                } else if (tile) {
                                    tile->addThing(item);
                                } else if (item->isGround()) {
                                    ground = item;
                                } else {
                                    tile = createTile(pos);
                                    tile->addThing(ground);
                                    tile->addThing(item);
                                }
                            } else {
                                g_logger.error(stdext::format("failed to unserialize item with %d at pos %d %d %d", item->getId(),
                                                              px, py, pz));
                                return false;
                            }
                        } else {
                            g_logger.error(stdext::format("Unknown item node type %d", type));
                            return false;
                        }
                    }

                    if (!tile) {
                        tile = createTile(pos);
                        tile->addThing(ground);
                    }

                    tile->setFlags((tileflags_t)flags);
                } else {
                    g_logger.error(stdext::format("Unknown tile node type %d", type));
                    return false;
                }
            }
        } else if (mapDataType == OTBM_TOWNS) {
            for (const BinaryTreePtr &nodeTown : nodeMapData->getChildren()) {
                if (nodeTown->getU8() == OTBM_TOWN) {
                    uint32 townId = nodeTown->getU32();
                    std::string townName = nodeTown->getString();

                    Position townCoords(nodeTown->getU16(), nodeTown->getU16(), nodeTown->getU8());
                } else
                    return false;
            }
        } else if (mapDataType == OTBM_WAYPOINTS && headerVersion > 1) {
            for (const BinaryTreePtr &nodeWaypoint : nodeMapData->getChildren()) {
                if (nodeWaypoint->getU8() == OTBM_WAYPOINT) {
                    std::string name = nodeWaypoint->getString();
                    Position waypointPos(nodeWaypoint->getU16(), nodeWaypoint->getU16(), nodeWaypoint->getU8());
                }
            }
        } else
            return false;
    }

    // TODO: Load house & spawns.
    return true;
}

bool Map::loadOtcm(const std::string& fileName)
{
    try {
        FileStreamPtr fin = g_resources.openFile(fileName);
        if(!fin)
            stdext::throw_exception("unable to open file");

        fin->cache();

        uint32 signature = fin->getU32();
        if(signature != OTCM_SIGNATURE)
            stdext::throw_exception("invalid otcm file");

        uint16 start = fin->getU16();
        uint16 version = fin->getU16();
        fin->getU32(); // flags

        switch(version) {
            case 1: {
                fin->getString(); // description
                uint32 datSignature = fin->getU32();
                fin->getU16(); // protocol version
                fin->getString(); // world name

                if(datSignature != g_things.getDatSignature())
                    g_logger.warning("otcm map loaded is was created with a different dat signature");

                break;
            }
            default:
                stdext::throw_exception("otcm version not supported");
                break;
        }

        fin->seek(start);

        while(true) {
            Position pos;

            pos.x = fin->getU16();
            pos.y = fin->getU16();
            pos.z = fin->getU8();

            // end of file
            if(!pos.isValid())
                break;

            while(true) {
                uint16 id = fin->getU16();

                // end of tile
                if(id == 0xFFFF)
                    break;

                uint8 countOrSubType = fin->getU8();

                ItemPtr item = Item::create(id);
                if(item->isStackable() || item->isFluidContainer() || item->isFluid())
                    item->setCountOrSubType(countOrSubType);

                if(item->isValid())
                    addThing(item, pos, 255);
            }
        }

        fin->close();

        return true;
    } catch(stdext::exception& e) {
        g_logger.error(stdext::format("failed to load OTCM map: %s", e.what()));
        return false;
    }
}

void Map::saveOtcm(const std::string& fileName)
{
    try {
        g_clock.update();

        FileStreamPtr fin = g_resources.createFile(fileName);
        fin->cache();

        //TODO: compression flag with zlib
        uint32 flags = 0;

        // header
        fin->addU32(OTCM_SIGNATURE);
        fin->addU16(0); // data start, will be overwritten later
        fin->addU16(OTCM_VERSION);
        fin->addU32(flags);

        // version 1 header
        fin->addString("OTCM 1.0"); // map description
        fin->addU32(g_things.getDatSignature());
        fin->addU16(g_game.getProtocolVersion());
        fin->addString(g_game.getWorldName());

        // go back and rewrite where the map data starts
        uint32 start = fin->tell();
        fin->seek(4);
        fin->addU16(start);
        fin->seek(start);

        for(auto& pair : m_tiles) {
            const TilePtr& tile = pair.second;
            if(!tile || tile->isEmpty())
                continue;

            Position pos = pair.first;
            fin->addU16(pos.x);
            fin->addU16(pos.y);
            fin->addU8(pos.z);

            const auto& list = tile->getThings();
            auto first = std::find_if(list.begin(), list.end(), [](const ThingPtr& thing) { return thing->isItem(); });
            for(auto it = first, end = list.end(); it != end; ++it) {
                ItemPtr item = (*it)->asItem();
                fin->addU16(item->getId());
                fin->addU8(item->getCountOrSubType());
            }

            // end of tile
            fin->addU16(0xFFFF);
        }

        // end of file
        Position invalidPos;
        fin->addU16(invalidPos.x);
        fin->addU16(invalidPos.y);
        fin->addU8(invalidPos.z);

        fin->flush();
        fin->close();
    } catch(stdext::exception& e) {
        g_logger.error(stdext::format("failed to save OTCM map: %s", e.what()));
    }
}

void Map::clean()
{
    cleanDynamicThings();
    m_tiles.clear();
}

void Map::cleanDynamicThings()
{
    for(const auto& pair : m_knownCreatures) {
        const CreaturePtr& creature = pair.second;
        removeThing(creature);
        creature->setRemoved(true);
    }
    m_knownCreatures.clear();

    for(int i=0;i<=Otc::MAX_Z;++i)
        m_floorMissiles[i].clear();

    cleanTexts();
}

void Map::cleanTexts()
{
    m_animatedTexts.clear();
    m_staticTexts.clear();
}

void Map::addThing(const ThingPtr& thing, const Position& pos, int stackPos)
{
    if(!thing)
        return;

    TilePtr tile = getOrCreateTile(pos);

    Position oldPos = thing->getPosition();

    if(thing->isItem() || thing->isCreature() || thing->isEffect()) {
        tile->addThing(thing, stackPos);
    } else if(thing->isMissile()) {
        m_floorMissiles[pos.z].push_back(thing->asMissile());
    } else if(thing->isAnimatedText()) {
        m_animatedTexts.push_back(thing->asAnimatedText());
    } else if(thing->isStaticText()) {
        StaticTextPtr staticText = thing->asStaticText();
        bool mustAdd = true;
        for(auto it = m_staticTexts.begin(), end = m_staticTexts.end(); it != end; ++it) {
            StaticTextPtr cStaticText = *it;
            if(cStaticText->getPosition() == pos) {
                // try to combine messages
                if(cStaticText->addMessage(staticText->getName(), staticText->getMessageType(), staticText->getFirstMessage())) {
                    mustAdd = false;
                    break;
                } else {
                    // must add another message and rearrenge current
                }
            }

        }

        if(mustAdd)
            m_staticTexts.push_back(staticText);
    }

    thing->startAnimation();
    thing->setPosition(pos);

    if(thing->isCreature()) {
        CreaturePtr creature = thing->asCreature();
        if(oldPos != pos) {
            if(oldPos.isInRange(pos,1,1))
                g_game.processCreatureMove(creature, oldPos, pos);
            else
                g_game.processCreatureTeleport(creature);
        }
    }

    notificateTileUpdateToMapViews(pos);
}

ThingPtr Map::getThing(const Position& pos, int stackPos)
{
    if(TilePtr tile = getTile(pos))
        return tile->getThing(stackPos);
    return nullptr;
}

bool Map::removeThing(const ThingPtr& thing)
{
    if(!thing)
        return false;

    notificateTileUpdateToMapViews(thing->getPosition());

    if(MissilePtr missile = thing->asMissile()) {
        int z = missile->getPosition().z;
        auto it = std::find(m_floorMissiles[z].begin(), m_floorMissiles[z].end(), missile);
        if(it != m_floorMissiles[z].end()) {
            m_floorMissiles[z].erase(it);
            return true;
        }
    } else if(AnimatedTextPtr animatedText = thing->asAnimatedText()) {
        auto it = std::find(m_animatedTexts.begin(), m_animatedTexts.end(), animatedText);
        if(it != m_animatedTexts.end()) {
            m_animatedTexts.erase(it);
            return true;
        }
    } else if(StaticTextPtr staticText = thing->asStaticText()) {
        auto it = std::find(m_staticTexts.begin(), m_staticTexts.end(), staticText);
        if(it != m_staticTexts.end()) {
            m_staticTexts.erase(it);
            return true;
        }
    } else if(TilePtr tile = thing->getTile())
        return tile->removeThing(thing);

    return false;
}

bool Map::removeThingByPos(const Position& pos, int stackPos)
{
    if(TilePtr tile = getTile(pos))
        return removeThing(tile->getThing(stackPos));
    return false;
}

TilePtr Map::createTile(const Position& pos)
{
    TilePtr tile = TilePtr(new Tile(pos));
    m_tiles[pos] = tile;
    return tile;
}

const TilePtr& Map::getTile(const Position& pos)
{
    auto it = m_tiles.find(pos);
    if(it != m_tiles.end())
        return it->second;
    static TilePtr nulltile;
    return nulltile;
}

TilePtr Map::getOrCreateTile(const Position& pos)
{
    const TilePtr& tile = getTile(pos);
    if(tile)
        return tile;
    else
        return createTile(pos);
}

void Map::cleanTile(const Position& pos)
{
    if(TilePtr tile = getTile(pos)) {
        tile->clean();
        if(tile->canErase())
            m_tiles.erase(m_tiles.find(pos));

        notificateTileUpdateToMapViews(pos);
    }
}

void Map::addCreature(const CreaturePtr& creature)
{
    m_knownCreatures[creature->getId()] = creature;
}

CreaturePtr Map::getCreatureById(uint32 id)
{
    auto it = m_knownCreatures.find(id);
    if(it == m_knownCreatures.end())
        return nullptr;
    return it->second;
}

void Map::removeCreatureById(uint32 id)
{
    if(id == 0)
        return;

    auto it = m_knownCreatures.find(id);
    if(it != m_knownCreatures.end())
        it->second->setRemoved(true);

    m_knownCreatures.erase(it);
}

void Map::setCentralPosition(const Position& centralPosition)
{
    m_centralPosition = centralPosition;

    // remove creatures from tiles that we are not aware anymore
    for(const auto& pair : m_knownCreatures) {
        const CreaturePtr& creature = pair.second;
        if(!isAwareOfPosition(creature->getPosition()))
            removeThing(creature);
    }

    // this fixes local player position when the local player is removed from the map,
    // the local player is removed from the map when there are too many creatures on his tile,
    // so there is no enough stackpos to the server send him
    g_dispatcher.addEvent([this] {
        LocalPlayerPtr localPlayer = g_game.getLocalPlayer();
        if(!localPlayer || localPlayer->getPosition() == m_centralPosition)
            return;
        TilePtr tile = localPlayer->getTile();
        if(tile && tile->hasThing(localPlayer))
            return;

        Position oldPos = localPlayer->getPosition();
        Position pos = m_centralPosition;
        localPlayer->setPosition(pos);
        if(oldPos != pos) {
            if(oldPos.isInRange(pos,1,1))
                g_game.processCreatureMove(localPlayer, oldPos, pos);
            else
                g_game.processCreatureTeleport(localPlayer);
        }
    });

    for(const MapViewPtr& mapView : m_mapViews)
        mapView->onMapCenterChange(centralPosition);
}

std::vector<CreaturePtr> Map::getSpectators(const Position& centerPos, bool multiFloor)
{
    return getSpectatorsInRange(centerPos, multiFloor, (Otc::VISIBLE_X_TILES - 1)/2, (Otc::VISIBLE_Y_TILES - 1)/2);
}

std::vector<CreaturePtr> Map::getSpectatorsInRange(const Position& centerPos, bool multiFloor, int xRange, int yRange)
{
    return getSpectatorsInRangeEx(centerPos, multiFloor, xRange, xRange, yRange, yRange);
}

std::vector<CreaturePtr> Map::getSpectatorsInRangeEx(const Position& centerPos, bool multiFloor, int minXRange, int maxXRange, int minYRange, int maxYRange)
{
    int minZRange = 0;
    int maxZRange = 0;
    std::vector<CreaturePtr> creatures;

    if(multiFloor) {
        minZRange = 0;
        maxZRange = Otc::MAX_Z;
    }

    //TODO: optimize
    //TODO: get creatures from other floors corretly
    //TODO: delivery creatures in distance order

    for(int iz=-minZRange; iz<=maxZRange; ++iz) {
        for(int iy=-minYRange; iy<=maxYRange; ++iy) {
            for(int ix=-minXRange; ix<=maxXRange; ++ix) {
                TilePtr tile = getTile(centerPos.translated(ix,iy,iz));
                if(!tile)
                    continue;

                auto tileCreatures = tile->getCreatures();
                creatures.insert(creatures.end(), tileCreatures.rbegin(), tileCreatures.rend());
            }
        }
    }

    return creatures;
}

bool Map::isLookPossible(const Position& pos)
{
    TilePtr tile = getTile(pos);
    return tile && tile->isLookPossible();
}

bool Map::isCovered(const Position& pos, int firstFloor)
{
    // check for tiles on top of the postion
    Position tilePos = pos;
    while(tilePos.coveredUp() && tilePos.z >= firstFloor) {
        TilePtr tile = getTile(tilePos);
        // the below tile is covered when the above tile has a full ground
        if(tile && tile->isFullGround())
            return true;
    }
    return false;
}

bool Map::isCompletelyCovered(const Position& pos, int firstFloor)
{
    Position tilePos = pos;
    while(tilePos.coveredUp() && tilePos.z >= firstFloor) {
        bool covered = true;
        // check in 2x2 range tiles that has no transparent pixels
        for(int x=0;x<2;++x) {
            for(int y=0;y<2;++y) {
                const TilePtr& tile = getTile(tilePos.translated(-x, -y));
                if(!tile || !tile->isFullyOpaque()) {
                    covered = false;
                    break;
                }
            }
        }
        if(covered)
            return true;
    }
    return false;
}

bool Map::isAwareOfPosition(const Position& pos)
{
    if(pos.z < getFirstAwareFloor() || pos.z > getLastAwareFloor())
        return false;

    Position groundedPos = pos;
    while(groundedPos.z != m_centralPosition.z) {
        if(groundedPos.z > m_centralPosition.z)
            groundedPos.coveredUp();
        else
            groundedPos.coveredDown();
    }
    return m_centralPosition.isInRange(groundedPos, Otc::AWARE_X_LEFT_TILES,
                                                    Otc::AWARE_X_RIGHT_TILES,
                                                    Otc::AWARE_Y_TOP_TILES,
                                                    Otc::AWARE_Y_BOTTOM_TILES);
}

int Map::getFirstAwareFloor()
{
    if(m_centralPosition.z > Otc::SEA_FLOOR)
        return m_centralPosition.z-Otc::AWARE_UNDEGROUND_FLOOR_RANGE;
    else
        return 0;
}

int Map::getLastAwareFloor()
{
    if(m_centralPosition.z > Otc::SEA_FLOOR)
        return std::min(m_centralPosition.z+Otc::AWARE_UNDEGROUND_FLOOR_RANGE, (int)Otc::MAX_Z);
    else
        return Otc::SEA_FLOOR;
}

std::tuple<std::vector<Otc::Direction>, Otc::PathFindResult> Map::findPath(const Position& startPos, const Position& goalPos, int maxSteps)
{
    // pathfinding using A* search algorithm
    // as described in http://en.wikipedia.org/wiki/A*_search_algorithm

    struct Node {
        Node(const Position& pos) : cost(0), totalCost(0), steps(0), pos(pos), prev(nullptr), dir(Otc::InvalidDirection), evaluated(false) { }
        bool operator<(const Node& other) const { return  totalCost < other.totalCost; }
        float cost;
        float totalCost;
        int steps;
        Position pos;
        Node *prev;
        Otc::Direction dir;
        bool evaluated;
    };

    struct LessNode : std::binary_function<Node*, Node*, bool> {
        bool operator()(Node* a, Node* b) const {
            return b->totalCost < a->totalCost;
        }
    };

    std::tuple<std::vector<Otc::Direction>, Otc::PathFindResult> ret;
    std::vector<Otc::Direction>& dirs = std::get<0>(ret);
    Otc::PathFindResult& result = std::get<1>(ret);

    result = Otc::PATHFIND_OK;

    if(startPos == goalPos) {
        result = Otc::PATHFIND_SAME_POSITION;
        return ret;
    }

    if(startPos.z != goalPos.z) {
        result = Otc::PATHFIND_IMPOSSIBLE;
        return ret;
    }

    if(startPos.distance(goalPos) > maxSteps) {
        result = Otc::PATHFIND_TOO_FAR;
        return ret;
    }

    std::unordered_map<Position, Node*, PositionHasher> nodes;
    std::priority_queue<Node*, std::vector<Node*>, LessNode> searchList;

    Node *currentNode = new Node(startPos);
    currentNode->pos = startPos;
    nodes[startPos] = currentNode;
    Node *foundNode = nullptr;
    while(currentNode && currentNode->steps < maxSteps) {
        if(currentNode->pos == goalPos && (!foundNode || currentNode->cost < foundNode->cost))
            foundNode = currentNode;

        if(foundNode && currentNode->totalCost >= foundNode->cost)
            break;

        for(int i=-1;i<=1;++i) {
            for(int j=-1;j<=1;++j) {
                if(i == 0 && j == 0)
                    continue;

                Position neighborPos = currentNode->pos.translated(i, j);
                const TilePtr& tile = getTile(neighborPos);
                if(!tile || (!tile->isPathable() && neighborPos != goalPos) || (!tile->isWalkable() && neighborPos == goalPos))
                    continue;

                float walkFactor;
                Otc::Direction walkDir = currentNode->pos.getDirectionFromPosition(neighborPos);
                if(walkDir >= Otc::NorthEast)
                    walkFactor = 3.0f;
                else
                    walkFactor = 1.0f;

                float cost = currentNode->cost + (tile->getGroundSpeed() * walkFactor) / 100.0f;

                Node *neighborNode;
                if(nodes.find(neighborPos) == nodes.end()) {
                    neighborNode = new Node(neighborPos);
                    nodes[neighborPos] = neighborNode;
                } else {
                    neighborNode = nodes[neighborPos];
                    if(neighborNode->cost < cost)
                        continue;
                }

                neighborNode->prev = currentNode;
                neighborNode->cost = cost;
                neighborNode->steps = currentNode->steps + 1;
                neighborNode->totalCost = neighborNode->cost + neighborPos.distance(goalPos);
                neighborNode->dir = walkDir;
                neighborNode->evaluated = false;
                searchList.push(neighborNode);
            }
        }

        currentNode->evaluated = true;
        currentNode = nullptr;
        while(searchList.size() > 0 && !currentNode) {
            Node *node = searchList.top();
            searchList.pop();

            if(!node->evaluated)
                currentNode = node;
        }
    }

    if(foundNode) {
        currentNode = foundNode;
        while(currentNode) {
            dirs.push_back(currentNode->dir);
            currentNode = currentNode->prev;
        }
        dirs.pop_back();
        std::reverse(dirs.begin(), dirs.end());
    } else
        result = Otc::PATHFIND_NO_WAY;

    for(auto it : nodes)
        delete it.second;

    return ret;
}
