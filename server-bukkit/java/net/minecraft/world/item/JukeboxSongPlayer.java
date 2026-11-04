// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item;

import java.util.*;
import java.util.function.*;
import java.util.stream.*;
import net.minecraft.core.*;
import net.minecraft.core.component.*;
import net.minecraft.network.chat.*;
import net.minecraft.server.level.*;
import net.minecraft.sounds.*;
import net.minecraft.world.*;
import net.minecraft.world.entity.*;
import net.minecraft.world.entity.player.*;
import net.minecraft.world.item.context.*;
import net.minecraft.world.level.*;
import net.minecraft.world.level.block.*;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.gameevent.GameEvent;
import net.minecraft.world.phys.*;
import net.minecraft.world.item.component.*;
import net.minecraft.world.entity.projectile.*;
import net.minecraft.world.entity.projectile.throwableitemprojectile.*;
import net.minecraft.world.entity.projectile.hurtingprojectile.*;
import net.minecraft.world.entity.projectile.arrow.*;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import net.minecraft.advancements.triggers.CriteriaTriggers;
import net.minecraft.stats.Stats;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;
import net.minecraft.world.item.enchantment.EnchantmentHelper;
import net.minecraft.world.level.block.entity.SignBlockEntity;
import net.minecraft.world.level.block.entity.SignTextSlot;
import net.minecraft.tags.*;

import net.minecraft.core.particles.ParticleTypes;
import net.minecraft.core.registries.Registries;
public class JukeboxSongPlayer {
    public static final int PLAY_EVENT_INTERVAL_TICKS = 20;
    private long ticksSinceSongStarted;
    private Holder<JukeboxSong> song;
    private final BlockPos blockPos;
    private final OnSongChanged onSongChanged;

    public JukeboxSongPlayer(OnSongChanged onSongChanged, BlockPos blockPos) {
        this.onSongChanged = onSongChanged;
        this.blockPos = blockPos;
    }

    public boolean isPlaying() {
        return song != null;
    }

    public JukeboxSong getSong() {
        return song == null ? null : song.value();
    }

    public long getTicksSinceSongStarted() {
        return ticksSinceSongStarted;
    }

    public void setSongWithoutPlaying(Holder<JukeboxSong> song, long ticksSinceSongStarted) {
        if (!song.value().hasFinished(ticksSinceSongStarted)) {
            this.song = song;
            this.ticksSinceSongStarted = ticksSinceSongStarted;
        }
    }

    public void play(LevelAccessor level, Holder<JukeboxSong> song) {
        this.song = song;
        this.ticksSinceSongStarted = 0L;
        int id = level.registryAccess().lookupOrThrow(Registries.JUKEBOX_SONG).getId(song.value());
        level.levelEvent(null, 1010, blockPos, id);
        onSongChanged.notifyChange();
    }

    public void stop(LevelAccessor level, BlockState blockState) {
        if (song == null) {
            return;
        }
        song = null;
        ticksSinceSongStarted = 0L;
        level.gameEvent(GameEvent.JUKEBOX_STOP_PLAY, blockPos, GameEvent.Context.of(blockState));
        level.levelEvent(1011, blockPos, 0);
        onSongChanged.notifyChange();
    }

    public void tick(LevelAccessor level, BlockState blockState) {
        if (song == null) {
            return;
        }
        if (song.value().hasFinished(ticksSinceSongStarted)) {
            stop(level, blockState);
            return;
        }
        if (ticksSinceSongStarted % PLAY_EVENT_INTERVAL_TICKS == 0L) {
            level.gameEvent(GameEvent.JUKEBOX_PLAY, blockPos, GameEvent.Context.of(blockState));
            spawnMusicParticles(level, blockPos);
        }
        ticksSinceSongStarted++;
    }

    private static void spawnMusicParticles(LevelAccessor level, BlockPos pos) {
        if (level instanceof ServerLevel serverLevel) {
            Vec3 at = Vec3.atBottomCenterOf(pos).add(0.0, 1.2F, 0.0);
            float color = (float) level.getRandom().nextInt(4) / 24.0F;
            serverLevel.sendParticles(ParticleTypes.NOTE, at.x(), at.y(), at.z(), 0, color, 0.0, 0.0, 1.0);
        }
    }

    @FunctionalInterface
    public interface OnSongChanged {
        void notifyChange();
    }
}
