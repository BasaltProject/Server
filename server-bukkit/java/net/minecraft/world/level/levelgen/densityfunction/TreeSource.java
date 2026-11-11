// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.densityfunction;

/** A density function that can state itself as game data JSON, so the native density code can evaluate it. */
public interface TreeSource {
    String treeJson();
}
