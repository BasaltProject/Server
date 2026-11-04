// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;

import java.util.List;

public interface BeaconBeamOwner {
    default List<Section> getBeamSections() {
        return List.of();
    }

    public static class Section {
        private final int color;
        private int height = 1;

        public Section(int color) {
            this.color = color;
        }

        public void increaseHeight() {
            height++;
        }

        public int getColor() {
            return color;
        }

        public int getHeight() {
            return height;
        }
    }
}
