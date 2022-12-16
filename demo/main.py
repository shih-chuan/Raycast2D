import pygame
from objects import World

pygame.init()

scr_w = 1200
scr_h = 900
clock = pygame.time.Clock()
screen = pygame.display.set_mode((scr_w, scr_h))
pygame.display.set_caption("Raycast2D")

running = True
world = World(screen)

while running:
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            running = False
        if event.type == pygame.MOUSEBUTTONUP:
            pos = pygame.mouse.get_pos()
            if event.button == 1:
                world.toggleWall(pos)
            if event.button == 3:
                world.lightOn = not world.lightOn
    world.light = pygame.mouse.get_pos()
    world.draw(screen)
    pygame.display.flip()
    clock.tick(150)