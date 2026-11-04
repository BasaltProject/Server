// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.lighting;

import net.minecraft.core.BlockPos;
import net.minecraft.core.SectionPos;
import net.minecraft.world.level.ChunkPos;
public interface LightEventListener {
    default void checkBlock(BlockPos pos) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.lighting.LightEventListener#checkBlock");
    }
    default boolean hasLightWork() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.lighting.LightEventListener#hasLightWork");
    }
    default int runLightUpdates() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.lighting.LightEventListener#runLightUpdates");
    }
    default void updateSectionStatus(BlockPos pos, boolean sectionEmpty) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.lighting.LightEventListener#updateSectionStatus");
    }
    default void updateSectionStatus(SectionPos pos, boolean sectionEmpty) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.lighting.LightEventListener#updateSectionStatus");
    }
    default void setLightEnabled(ChunkPos pos, boolean enable) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.lighting.LightEventListener#setLightEnabled");
    }
    default void propagateLightSources(ChunkPos pos) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.lighting.LightEventListener#propagateLightSources");
    }
}
