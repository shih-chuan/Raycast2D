import unittest
from _raycast2d import litArea, litArea_naive, WallList

class RaycastTest(unittest.TestCase):
    # test case 1: basic test
    def test_basic(self):
        walls = WallList()
        wall_sx, wall_sy, wall_ex, wall_ey = 50, 50, 100, 100
        walls.append(wall_sx, wall_sy, wall_ex, wall_ey)
        polygonData = litArea(30.0, 40.0, walls)
        groundTruth = [50, 50, 100, 100, 100, 100]
        for d in range(0, len(polygonData)):
            self.assertAlmostEqual(groundTruth[d], polygonData[d], delta=0.1)
    # test case 2: test occlusion
    def test_occlusion(self):
        walls = WallList()
        walls.append(50, 100, 100, 100)
        walls.append(40, 110, 120, 110)
        polygonData = litArea(80, 120, walls)
        for d in range(0, len(polygonData), 2):
            self.assertGreaterEqual(polygonData[d], 40)
            self.assertLessEqual(polygonData[d], 120)
            self.assertEqual(polygonData[d + 1], 110)
    # test case 3: test empty wall list
    def test_empty(self):
        walls = WallList()
        polygonData = litArea(30.0, 40.0, walls)
        print(polygonData)
    # test case 4: light on the wall
    def test_light_on_wall(self):
        walls = WallList()
        walls.append(50, 50, 100, 100)
        polygonData = litArea(60.0, 60.0, walls)
        for d in range(0, len(polygonData)):
            self.assertEqual(60, polygonData[d])
    # test case 5: test simd
    def test_simd(self):
        walls = WallList()
        walls.append(-10, -10, (900 + 10), -10)
        walls.append((900 + 10), -10, (900 + 10), (1200 + 10))
        walls.append((900 + 10), (1200 + 10), -10, (1200 + 10)) 
        walls.append(-10, (1200 + 10), -10, -10)
        walls.append(50, 50, 100, 100)
        walls.append(30, 30, 40, 60)
        results = litArea_naive(50.0, 50.0, walls)
        simd_results = litArea(50.0, 50.0, walls)
        self.assertEqual(results, simd_results)
        print(results)
        print(simd_results)
if __name__ == "__main__":
    testcases = RaycastTest()
    # testcases.test_basic()
    testcases.test_light_on_wall()
