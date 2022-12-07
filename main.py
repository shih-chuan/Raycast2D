import pygame
import math
import config
import random
# from objects import Boundary, Ray, Light
from _raycast2d import Boundary, Light, Map

pygame.init()

scr_w = 1000
scr_h = 1000
clock = pygame.time.Clock()
screen = pygame.display.set_mode((scr_w, scr_h))
light = pygame.image.load('assets/light_cast.png') # radial gradient used for light pattern
light = pygame.transform.scale(light, (2000,2000)) # resize gradient
pygame.display.set_caption("Raycast2D")

running = True

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

while running:
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            running = False
    screen.fill(config.BLACK)
    for wall in walls:
        a = (wall.a.x, wall.a.y)
        b = (wall.b.x, wall.b.y)
        pygame.draw.line(screen, config.WHITE, a, b, 2)

    map.light.move(*pygame.mouse.get_pos())
    rays = map.visibility_polygon()
    polygon = []
    for r in range(0, len(rays), 2):
        polygon.append((rays[r], rays[r+1]))
        # pygame.draw.line(
        #     screen, 
        #     pygame.Color(200, 200, 200, 10), 
        #     (map.light.pos.x, map.light.pos.y), 
        #     (rays[r], rays[r+1]), 
        # )
    filter = pygame.surface.Surface((scr_w, scr_h)) # create surface same size as window
    filter.fill(pygame.color.Color('Black')) # Black will give dark unlit areas, Grey will give you a fog
    pygame.draw.polygon(filter, config.WHITE, polygon)
    filter.blit(light,(map.light.pos.x - 1000, map.light.pos.y - 1000), special_flags=pygame.BLEND_MIN) # blit light to the filter surface -400 is to center effect
    screen.blit(filter, (0, 0), special_flags=pygame.BLEND_RGBA_MAX) # blit filter surface but with a blend
 

    pygame.display.flip()
    clock.tick(150)