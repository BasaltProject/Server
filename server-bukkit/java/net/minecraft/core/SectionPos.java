package net.minecraft.core;

import net.minecraft.world.level.ChunkPos;
import org.basaltmc.bukkit.Unsupported;
public class SectionPos extends Vec3i {
    protected SectionPos() {
    }
    public static int blockToSectionCoord(int blockCoord) {
        throw Unsupported.hit("net.minecraft.core.SectionPos.blockToSectionCoord");
    }
    public static SectionPos of(ChunkPos pos, int sectionY) {
        throw Unsupported.hit("net.minecraft.core.SectionPos.of");
    }
}
