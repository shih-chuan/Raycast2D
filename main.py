import pygame
import math
import config
import random
from objects import Boundary, Ray, Light

pygame.init()

scr_w = 1000
scr_h = 1000
clock = pygame.time.Clock()
screen = pygame.display.set_mode((scr_w, scr_h))
pygame.display.set_caption("Raycast2D")

running = True

ray = Ray((150, 150), math.radians(180))
walls = [Boundary(
    random.randint(0, scr_w), 
    random.randint(0, scr_h), 
    random.randint(0, scr_w), 
    random.randint(0, scr_h), 
) for i in range(100)]

walls.append(Boundary(0, 0, scr_w, 0))
walls.append(Boundary(scr_w, 0, scr_w, scr_h))
walls.append(Boundary(scr_w, scr_h, 0, scr_h))
walls.append(Boundary(0, scr_h, 0, 0))

# wall = Boundary(600, 100, 600, 400)
light = Light(150, 150)

while running:
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            running = False
    screen.fill(config.BLACK)
    for wall in walls:
        wall.draw(screen, config.WHITE)
    light.move(*pygame.mouse.get_pos())
    light.look(screen, config.WHITE, walls)
    light.draw(screen, config.WHITE)
    # ray.lookAt(*pygame.mouse.get_pos())
    # ray.draw(screen, config.WHITE)
    # pt = ray.cast(wall)
    # if pt:
    #     pygame.draw.circle(screen, config.WHITE, pt, 5)

    pygame.display.update()
    clock.tick(30)