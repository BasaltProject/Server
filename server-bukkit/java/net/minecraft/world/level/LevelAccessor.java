// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;

import net.minecraft.core.RegistryAccess;
import org.basaltmc.bukkit.Unsupported;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.core.Holder;
import net.minecraft.core.particles.ParticleOptions;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.MinecraftServer;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.sounds.SoundSource;
import net.minecraft.util.RandomSource;
import net.minecraft.world.Difficulty;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.ChunkSource;
import net.minecraft.world.level.gameevent.GameEvent;
import net.minecraft.world.level.storage.LevelData;
import net.minecraft.world.phys.Vec3;
import net.minecraft.world.ticks.ScheduledTick;
import net.minecraft.world.ticks.TickPriority;
public interface LevelAccessor extends CommonLevelAccessor, ScheduledTickAccess {
    public default RegistryAccess registryAccess() {
        throw Unsupported.hit("net.minecraft.world.level.LevelAccessor.registryAccess");
    }
    default long nextSubTickCount() {
        return this instanceof Level level ? level.nextSubTickCount() : 0L;
    }
    default <T> ScheduledTick<T> createTick(BlockPos pos, T type, int tickDelay, TickPriority priority) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.LevelAccessor#createTick");
    }
    default <T> ScheduledTick<T> createTick(BlockPos pos, T type, int tickDelay) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.LevelAccessor#createTick");
    }
    default LevelData getLevelData() {
        if (this instanceof Level level) {
            return level.getLevelData();
        }
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.LevelAccessor#getLevelData");
    }
    default long getGameTime() {
        return this instanceof Level level ? level.getWorld().getGameTime() : 0L;
    }
    default MinecraftServer getServer() {
        return net.minecraft.server.MinecraftServer.getServer();
    }
    default Difficulty getDifficulty() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.LevelAccessor#getDifficulty");
    }
    default ChunkSource getChunkSource() {
        if (this instanceof Level level) {
            return level.getChunkSource();
        }
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.LevelAccessor#getChunkSource");
    }
    default boolean hasChunk(int chunkX, int chunkZ) {
        return getChunkSource().hasChunk(chunkX, chunkZ);
    }
    default RandomSource getRandom() {
        if (this instanceof Level level) {
            return level.getRandom();
        }
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.LevelAccessor#getRandom");
    }
    default void updateNeighborsAt(BlockPos pos, Block sourceBlock) {

    }
    default void neighborShapeChanged(Direction direction, BlockPos pos, BlockPos neighborPos, BlockState neighborState, int updateFlags, int updateLimit) {

    }
    default void playSound(Entity except, BlockPos pos, SoundEvent soundEvent, SoundSource source) {
        playSound(except, pos, soundEvent, source, 1.0F, 1.0F);
    }
    default void playSound(Entity except, BlockPos pos, SoundEvent sound, SoundSource source, float volume, float pitch) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.LevelAccessor#playSound");
    }
    default void addParticle(ParticleOptions particle, double x, double y, double z, double xd, double yd, double zd) {

    }
    default void levelEvent(Entity source, int type, BlockPos pos, int data) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.LevelAccessor#levelEvent");
    }
    default void levelEvent(int type, BlockPos pos, int data) {
        levelEvent(null, type, pos, data);
    }
    default void gameEvent(Holder<GameEvent> gameEvent, Vec3 position, GameEvent.Context context) {

    }
    default void gameEvent(Entity sourceEntity, Holder<GameEvent> gameEvent, Vec3 pos) {

    }
    default void gameEvent(Entity sourceEntity, Holder<GameEvent> gameEvent, BlockPos pos) {

    }
    default void gameEvent(Holder<GameEvent> gameEvent, BlockPos pos, GameEvent.Context context) {

    }
    default void gameEvent(ResourceKey<GameEvent> gameEvent, BlockPos pos, GameEvent.Context context) {

    }
}
