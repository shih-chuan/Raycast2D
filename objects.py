import pygame
import math, config
from _raycast2d import Boundary

NORTH, SOUTH, EAST, WEST = 0, 1, 2, 3

def normalize(vector):
    rms = math.sqrt(vector[0]**2 + vector[1]**2)
    vector = (vector[0] / rms, vector[1] / rms)
    return vector

def distance(p1, p2):
    return math.sqrt((p1[0] - p2[0])**2 + (p1[1] - p2[1])**2)

class sEdge:
    def __init__(self, x1, y1, x2, y2) -> None:
        self.a.x, self.a.y = x1, y1
        self.b.x, self.ey = x2, y2

class sCell:
    def __init__(self) -> None:
        self.edge_id = [0] * 4
        self.edge_exist = [False] * 4
        self.exist = False

class World:
    def __init__(self, screen) -> None:
        self.nCellsWidth = 40
        self.nCellsHeight = 30
        self.blockWidth = screen.get_width() // self.nCellsWidth
        self.blockHeight = screen.get_height() // self.nCellsHeight
        self.world = [sCell() for i in range(0, self.nCellsHeight * self.nCellsWidth)]
        self.boundaries = []
        print(self.world[0].exist)
    def toPolyMap(self):
        self.boundaries.clear()
        for row in range(0, self.nCellsHeight):
            for col in range(0, self.nCellsWidth):
                for j in range(0, 4):
                    self.world[row * self.nCellsWidth + col].edge_exist[j] = False
                    self.world[row * self.nCellsWidth + col].edge_id[j] = 0
        for row in range(1, self.nCellsHeight - 1):
            for col in range(1, self.nCellsWidth - 1):
                cellID = row * self.nCellsWidth + col
                n = (row - 1) * self.nCellsWidth + col
                s = (row + 1) * self.nCellsWidth + col
                w = row * self.nCellsWidth + (col - 1)
                e = row * self.nCellsWidth + (col + 1)
                if self.world[cellID].exist:
                    if not self.world[w].exist:
                        if self.world[n].edge_exist[WEST]:
                            self.boundaries[self.world[n].edge_id[WEST]].b.y = self.boundaries[self.world[n].edge_id[WEST]].b.y + self.blockHeight
                            self.world[cellID].edge_id[WEST] = self.world[n].edge_id[WEST]
                            self.world[cellID].edge_exist[WEST] = True
                        else:
                            edge = Boundary(col * self.blockWidth, row * self.blockHeight, col * self.blockWidth, (row + 1) * self.blockHeight)
                            edge_id = len(self.boundaries)
                            self.boundaries.append(edge)
                            self.world[cellID].edge_id[WEST] = edge_id
                            self.world[cellID].edge_exist[WEST] = True
                    if not self.world[e].exist:
                        if self.world[n].edge_exist[EAST]:
                            self.boundaries[self.world[n].edge_id[EAST]].b.y = self.boundaries[self.world[n].edge_id[EAST]].b.y + self.blockHeight
                            self.world[cellID].edge_id[EAST] = self.world[n].edge_id[EAST]
                            self.world[cellID].edge_exist[EAST] = True
                        else:
                            edge = Boundary((col + 1) * self.blockWidth, row * self.blockHeight, (col + 1) * self.blockWidth, (row + 1) * self.blockHeight)
                            edge_id = len(self.boundaries)
                            self.boundaries.append(edge)
                            self.world[cellID].edge_id[EAST] = edge_id
                            self.world[cellID].edge_exist[EAST] = True
                    if not self.world[n].exist:
                        if self.world[w].edge_exist[NORTH]:
                            self.boundaries[self.world[w].edge_id[NORTH]].b.x = self.boundaries[self.world[w].edge_id[NORTH]].b.x + self.blockWidth
                            self.world[cellID].edge_id[NORTH] = self.world[w].edge_id[NORTH]
                            self.world[cellID].edge_exist[NORTH] = True
                        else:
                            edge = Boundary(col * self.blockWidth, row * self.blockHeight, (col + 1) * self.blockWidth, row * self.blockHeight)
                            edge_id = len(self.boundaries)
                            self.boundaries.append(edge)
                            self.world[cellID].edge_id[NORTH] = edge_id
                            self.world[cellID].edge_exist[NORTH] = True
                    if not self.world[s].exist:
                        if self.world[w].edge_exist[SOUTH]:
                            self.boundaries[self.world[w].edge_id[SOUTH]].b.x = self.boundaries[self.world[w].edge_id[SOUTH]].b.x + self.blockWidth
                            self.world[cellID].edge_id[SOUTH] = self.world[w].edge_id[SOUTH]
                            self.world[cellID].edge_exist[SOUTH] = True
                        else:
                            edge = Boundary(col * self.blockWidth, (row + 1) * self.blockHeight, (col + 1) * self.blockWidth, (row + 1) * self.blockHeight)
                            edge_id = len(self.boundaries)
                            self.boundaries.append(edge)
                            self.world[cellID].edge_id[SOUTH] = edge_id
                            self.world[cellID].edge_exist[SOUTH] = True

    def toggleWall(self, mouse_pos):
        mouse_x, mouse_y = mouse_pos
        cellID = ((mouse_y // self.blockHeight) * self.nCellsWidth) + (mouse_x // self.blockWidth)
        print("clicked:", cellID)
        self.world[cellID].exist = not self.world[cellID].exist
        self.toPolyMap()

    def draw(self, screen):
        for row in range(0, self.nCellsHeight):
            for col in range(0, self.nCellsWidth):
                if self.world[row * self.nCellsWidth + col].exist:
                    screen.fill(config.BLUE, pygame.Rect(col * self.blockWidth, row * self.blockHeight, self.blockWidth, self.blockHeight))
        for edge in self.boundaries:
            pygame.draw.line(screen, config.WHITE, (edge.a.x, edge.a.y), (edge.b.x, edge.b.y), 2)

