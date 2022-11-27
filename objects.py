import pygame
import math
import _raycast2d

# class Vector:
#     def __init__(self, x, y) -> None:
#         self.x = x
#         self.y = y

def normalize(vector):
    rms = math.sqrt(vector[0]**2 + vector[1]**2)
    vector = (vector[0] / rms, vector[1] / rms)
    return vector

def distance(p1, p2):
    return math.sqrt((p1[0] - p2[0])**2 + (p1[1] - p2[1])**2)

class Boundary:
    def __init__(self, x1, y1, x2, y2) -> None:
        # self.boundary = raycast2d.Boundary(x1, y1, x2, y2);
        self.a = (x1, y1)
        self.b = (x2, y2)
    def draw(self, screen, color, width=1):
        # pygame.draw.line(screen, color, (self.boundary.a.x, self.boundary.a.y), (self.boundary.b.x, self.boundary.b.y), width)
        pygame.draw.line(screen, color, self.a, self.b, width)

class Ray:
    def __init__(self, pos, angle) -> None:
        self.pos = pos
        self.dir = normalize((math.cos(angle), math.sin(angle)))

    def lookAt(self, x, y):
        self.dir = (x - self.pos[0], y - self.pos[1])
        self.dir = normalize(self.dir)

    def cast(self, wall: Boundary):
        x1, y1 = wall.a
        x2, y2 = wall.b
        x3, y3 = self.pos
        x4, y4 = self.pos[0] + self.dir[0], self.pos[1] + self.dir[1]
        den = (x1 - x2) * (y3 - y4) - (y1 - y2) *(x3 - x4)
        if den == 0:
            return
        t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / den
        u = -((x1 - x2) * (y1 - y3) - (y1 - y2) * (x1 - x3)) / den
        if t > 0 and t < 1 and u > 0:
            pt = (x1 + t * (x2 - x1), y1 + t * (y2 - y1))
            return pt
        else:
            return

    def draw(self, screen, color):
        pygame.draw.line(screen, color, self.pos, (self.pos[0] + self.dir[0] * 10, self.pos[1] + self.dir[1] * 10))

class Light:
    def __init__(self, x, y) -> None:
        self.pos = (x, y)
        self.rays = [Ray((x, y), math.radians(i)) for i in range(0, 360, 1)]

    def move(self, x, y):
        self.pos = (x, y)
        self.rays = [Ray((x, y), math.radians(i)) for i in range(0, 360, 1)]

    def look(self, screen, color, walls):
        for ray in self.rays:
            nearest = None
            record = float("inf")
            for wall in walls:
                pt = ray.cast(wall)
                if pt:
                    d = distance(self.pos, pt)
                    if d < record:
                        record = d
                        nearest = pt
            if nearest:
                pygame.draw.line(screen, pygame.Color(200, 200, 200, 10), self.pos, nearest)

    def draw(self, screen, color):
        for ray in self.rays:
            ray.draw(screen, color)
            pygame.draw.circle(screen, color, self.pos, 16)
