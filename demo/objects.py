import pygame, random
import config
from _raycast2d import litArea, castRays, FloatVector

lightMask = pygame.image.load('assets/light_cast.png') # radial gradient used for light pattern
lightMask = pygame.transform.scale(lightMask, (2400,2400)) # resize gradient
NORTH, SOUTH, EAST, WEST = 0, 1, 2, 3
x1, y1, x2, y2 = 0, 1, 2, 3

class Cell:
    def __init__(self) -> None:
        self.edge_id = [0] * 4
        self.edge_exist = [False] * 4
        self.exist = False

class World:
    def __init__(self, screen) -> None:
        self.mapWidth = screen.get_width()
        self.mapHeight = screen.get_height()
        self.nCellsWidth = 40
        self.nCellsHeight = 30
        self.blockWidth = self.mapWidth // self.nCellsWidth
        self.blockHeight = self.mapHeight // self.nCellsHeight
        self.world = [Cell() for i in range(0, (self.nCellsHeight + 1) * self.nCellsWidth)]
        self.light = (0, 0)
        self.lightOn = False
        self.raysOn = False
        self.nRays = 360
        self.lightMaskOn = False
        self.walls = FloatVector()
        self.walls.extend([
            -10, -10, (self.mapWidth + 10), -10, 
            (self.mapWidth + 10), -10, (self.mapWidth + 10), (self.mapHeight + 10), 
            (self.mapWidth + 10), (self.mapHeight + 10), -10, (self.mapHeight + 10), 
            -10, (self.mapHeight + 10), -10, -10
        ])
    def toPolyMap(self):
        self.walls.clear()
        for row in range(0, self.nCellsHeight):
            for col in range(0, self.nCellsWidth):
                for j in range(0, 4):
                    self.world[row * self.nCellsWidth + col].edge_exist[j] = False
                    self.world[row * self.nCellsWidth + col].edge_id[j] = 0
        for row in range(0, self.nCellsHeight):
            for col in range(0, self.nCellsWidth):
                cellID = row * self.nCellsWidth + col
                n = (row - 1) * self.nCellsWidth + col
                s = (row + 1) * self.nCellsWidth + col
                w = row * self.nCellsWidth + (col - 1)
                e = row * self.nCellsWidth + (col + 1)
                if self.world[cellID].exist:
                    if not self.world[w].exist:
                        if self.world[n].edge_exist[WEST]:
                            self.walls[self.world[n].edge_id[WEST] * 4 + 3] = self.walls[self.world[n].edge_id[WEST] * 4 + 3] + self.blockHeight
                            self.world[cellID].edge_id[WEST] = self.world[n].edge_id[WEST]
                            self.world[cellID].edge_exist[WEST] = True
                        else:
                            edge = [col * self.blockWidth, row * self.blockHeight, col * self.blockWidth, (row + 1) * self.blockHeight]
                            edge_id = len(self.walls) // 4
                            self.walls.extend(edge)
                            self.world[cellID].edge_id[WEST] = edge_id
                            self.world[cellID].edge_exist[WEST] = True
                    if not self.world[e].exist:
                        if self.world[n].edge_exist[EAST]:
                            self.walls[self.world[n].edge_id[EAST] * 4 + 3] = self.walls[self.world[n].edge_id[EAST] * 4 + 3] + self.blockHeight
                            self.world[cellID].edge_id[EAST] = self.world[n].edge_id[EAST]
                            self.world[cellID].edge_exist[EAST] = True
                        else:
                            edge = [(col + 1) * self.blockWidth, row * self.blockHeight, (col + 1) * self.blockWidth, (row + 1) * self.blockHeight]
                            edge_id = len(self.walls) // 4
                            self.walls.extend(edge)
                            self.world[cellID].edge_id[EAST] = edge_id
                            self.world[cellID].edge_exist[EAST] = True
                    if not self.world[n].exist:
                        if self.world[w].edge_exist[NORTH]:
                            self.walls[self.world[w].edge_id[NORTH] * 4 + 2] = self.walls[self.world[w].edge_id[NORTH] * 4 + 2] + self.blockWidth
                            self.world[cellID].edge_id[NORTH] = self.world[w].edge_id[NORTH]
                            self.world[cellID].edge_exist[NORTH] = True
                        else:
                            edge = [col * self.blockWidth, row * self.blockHeight, (col + 1) * self.blockWidth, row * self.blockHeight]
                            edge_id = len(self.walls) // 4
                            self.walls.extend(edge)
                            self.world[cellID].edge_id[NORTH] = edge_id
                            self.world[cellID].edge_exist[NORTH] = True
                    if not self.world[s].exist:
                        if self.world[w].edge_exist[SOUTH]:
                            self.walls[self.world[w].edge_id[SOUTH] * 4 + 2] = self.walls[self.world[w].edge_id[SOUTH] * 4 + 2] + self.blockWidth
                            self.world[cellID].edge_id[SOUTH] = self.world[w].edge_id[SOUTH]
                            self.world[cellID].edge_exist[SOUTH] = True
                        else:
                            edge = [col * self.blockWidth, (row + 1) * self.blockHeight, (col + 1) * self.blockWidth, (row + 1) * self.blockHeight]
                            edge_id = len(self.walls) // 4
                            self.walls.extend(edge)
                            self.world[cellID].edge_id[SOUTH] = edge_id
                            self.world[cellID].edge_exist[SOUTH] = True
        self.walls.extend([
            -10, -10, (self.mapWidth + 10), -10, 
            (self.mapWidth + 10), -10, (self.mapWidth + 10), (self.mapHeight + 10), 
            (self.mapWidth + 10), (self.mapHeight + 10), -10, (self.mapHeight + 10), 
            -10, (self.mapHeight + 10), -10, -10
        ])

    def toggleWall(self, mouse_pos):
        mouse_x, mouse_y = mouse_pos
        cellID = ((mouse_y // self.blockHeight) * self.nCellsWidth) + (mouse_x // self.blockWidth)
        self.world[cellID].exist = not self.world[cellID].exist
        self.toPolyMap()

    def clearWalls(self):
        self.walls.clear()
        for cell in self.world:
            cell.exist = False
        self.toPolyMap()

    def randomWalls(self, n):
        list = range(1, self.nCellsHeight * self.nCellsWidth)
        for cell in random.sample(list, n):
            self.world[cell].exist = True
        self.toPolyMap()

    def draw(self, screen):
        # draw background
        screen.fill(config.BLACK)
        # draw grid
        for x in range(0, self.mapWidth, self.blockWidth):
            for y in range(0, self.mapHeight, self.blockHeight):
                rect = pygame.Rect(x, y, self.blockWidth, self.blockHeight)
                pygame.draw.rect(screen, config.GREY, rect, 1)
        # draw walls
        for row in range(0, self.nCellsHeight):
            for col in range(0, self.nCellsWidth):
                if self.world[row * self.nCellsWidth + col].exist:
                    screen.fill(config.BLUE, pygame.Rect(col * self.blockWidth, row * self.blockHeight, self.blockWidth, self.blockHeight))
        for i in range(0, len(self.walls), 4):
            pygame.draw.line(screen, config.WHITE, (self.walls[i], self.walls[i + 1]), (self.walls[i + 2], self.walls[i + 3]), 2)
        # draw rays
        if self.raysOn:
            rays = castRays(float(self.light[0]), float(self.light[1]), self.walls, self.nRays)
            for d in range(0, len(rays), 2):
                pygame.draw.line(screen, config.WHITE, (float(self.light[0]), float(self.light[1])), (rays[d], rays[d+1]), 2)
        # draw lit area
        if self.lightOn:
            polygonData = litArea(float(self.light[0]), float(self.light[1]), self.walls)
            polygon = []
            for d in range(0, len(polygonData), 2):
                polygon.append((polygonData[d], polygonData[d+1]))
            if len(polygon) > 2:
                if self.lightMaskOn:
                    filter = pygame.surface.Surface((screen.get_width(), screen.get_height()))
                    filter.fill(pygame.color.Color('Black'))
                    pygame.draw.polygon(filter, config.WHITE, polygon)
                    filter.blit(lightMask, (self.light[0] - 1200, self.light[1] - 1200), special_flags=pygame.BLEND_MIN)
                    screen.blit(filter, (0, 0), special_flags=pygame.BLEND_RGBA_MAX)
                else:
                    pygame.draw.polygon(screen, config.WHITE, polygon)

