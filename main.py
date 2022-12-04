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
pygame.display.set_caption("Raycast2D")

running = True

walls = [Boundary(
    random.randint(0, scr_w), 
    random.randint(0, scr_h), 
    random.randint(0, scr_w), 
    random.randint(0, scr_h), 
) for i in range(5)]
walls.append(Boundary(0, 0, scr_w, 0))
walls.append(Boundary(scr_w, 0, scr_w, scr_h))
walls.append(Boundary(scr_w, scr_h, 0, scr_h))
walls.append(Boundary(0, scr_h, 0, 0))

# wall = Boundary(600, 100, 600, 400)
light = Light(500, 500)
map = Map(light)
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
    rays = map.light_cast()
    for r in range(0, len(rays), 2):
        pygame.draw.line(
            screen, 
            pygame.Color(200, 200, 200, 10), 
            (map.light.pos.x, map.light.pos.y), 
            (rays[r], rays[r+1]), 
        )

    pygame.display.update()
    clock.tick(150)