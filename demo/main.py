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
        elif event.type == pygame.MOUSEBUTTONUP:
            pos = pygame.mouse.get_pos()
            if event.button == 1:
                world.toggleWall(pos)
            if event.button == 2:
                world.raysOn = not world.raysOn
            if event.button == 3:
                world.lightOn = not world.lightOn
        elif event.type == pygame.MOUSEWHEEL:
            world.nRays += event.y * 10
        elif event.type == pygame.KEYUP:
            if event.key == pygame.K_SPACE:
                world.lightMaskOn = not world.lightMaskOn
    world.light = pygame.mouse.get_pos()
    world.draw(screen)
    pygame.display.flip()
    clock.tick(150)