import pygame
import math
import config
import random
from objects import World
from _raycast2d import Boundary, Light, Map

pygame.init()

scr_w = 1200
scr_h = 900
clock = pygame.time.Clock()
screen = pygame.display.set_mode((scr_w, scr_h))
light = pygame.image.load('assets/light_cast.png') # radial gradient used for light pattern
light = pygame.transform.scale(light, (2400,2400)) # resize gradient
pygame.display.set_caption("Raycast2D")

running = True
world = World(screen)
walls = [Boundary(
    random.randint(0, scr_w), 
    random.randint(0, scr_h), 
    random.randint(0, scr_w), 
    random.randint(0, scr_h), 
) for i in range(3)]
# walls = [
#     Boundary(250, 100, 250, 900),
#     Boundary(250, 900, 900, 900)
# ]
walls.append(Boundary(-10, -10, scr_w, -10))
walls.append(Boundary(scr_w, -10, scr_w, scr_h))
walls.append(Boundary(scr_w, scr_h, -10, scr_h))
walls.append(Boundary(-10, scr_h, -10, -10))

# wall = Boundary(600, 100, 600, 400)
mlight = Light(500, 500)
map = Map(mlight)
for wall in walls:
    map.add_wall(wall)
map.walls = []

def drawGrid(screen, blockSize):
    blockSize = 30 #Set the size of the grid block
    for x in range(0, scr_w, blockSize):
        for y in range(0, scr_h, blockSize):
            rect = pygame.Rect(x, y, blockSize, blockSize)
            pygame.draw.rect(screen, config.GREY, rect, 1)
            

while running:
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            running = False
        if event.type == pygame.MOUSEBUTTONUP:
            pos = pygame.mouse.get_pos()
            world.toggleWall(pos)

    map.walls = []
    for wall in world.boundaries:
        map.add_wall(wall)
        map.add_wall(Boundary(-10, -10, scr_w, -10))
        map.add_wall(Boundary(scr_w, -10, scr_w, scr_h))
        map.add_wall(Boundary(scr_w, scr_h, -10, scr_h))
        map.add_wall(Boundary(-10, scr_h, -10, -10))
    screen.fill(config.BLACK)
    drawGrid(screen, 5)
    world.draw(screen)
    # for wall in walls:
    #     a = (wall.a.x, wall.a.y)
    #     b = (wall.b.x, wall.b.y)
    #     pygame.draw.line(screen, config.WHITE, a, b, 2)
    map.light.move(*pygame.mouse.get_pos())
    rays = map.visibility_polygon()
    polygon = []
    for r in range(0, len(rays), 2):
        polygon.append((rays[r], rays[r+1]))
    filter = pygame.surface.Surface((scr_w, scr_h)) # create surface same size as window
    filter.fill(pygame.color.Color('Black')) # Black will give dark unlit areas, Grey will give you a fog
    if len(polygon) > 2:
        pygame.draw.polygon(filter, config.WHITE, polygon)
    filter.blit(light,(map.light.pos.x - 1200, map.light.pos.y - 1200), special_flags=pygame.BLEND_MIN) # blit light to the filter surface -400 is to center effect
    screen.blit(filter, (0, 0), special_flags=pygame.BLEND_RGBA_MAX) # blit filter surface but with a blend
    # if len(polygon) > 2:
    #     pygame.draw.polygon(screen, config.WHITE, polygon)
 

    pygame.display.flip()
    clock.tick(150)