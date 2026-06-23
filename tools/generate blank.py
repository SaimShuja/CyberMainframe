import random

# Grid dimensions matching your BRAM layout (100x60 = 6000 cells)
WIDTH = 100
HEIGHT = 56
TOTAL_CELLS = WIDTH * HEIGHT

DIRT = 0x0

def generate_random_forest():
    with open("mainframe_init.hex", "w") as f:
        for _ in range(TOTAL_CELLS):
                f.write(f"{DIRT:X}\n")

    print(f"Success! 'mainframe_init.hex' generated with {TOTAL_CELLS} cells.")

if __name__ == "__main__":
    generate_random_forest()