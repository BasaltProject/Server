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

import net.minecraft.core.registries.Registries;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
public interface JukeboxSongs {
    ResourceKey<JukeboxSong> THIRTEEN = create("13");
    ResourceKey<JukeboxSong> CAT = create("cat");
    ResourceKey<JukeboxSong> BLOCKS = create("blocks");
    ResourceKey<JukeboxSong> CHIRP = create("chirp");
    ResourceKey<JukeboxSong> FAR = create("far");
    ResourceKey<JukeboxSong> MALL = create("mall");
    ResourceKey<JukeboxSong> MELLOHI = create("mellohi");
    ResourceKey<JukeboxSong> STAL = create("stal");
    ResourceKey<JukeboxSong> STRAD = create("strad");
    ResourceKey<JukeboxSong> WARD = create("ward");
    ResourceKey<JukeboxSong> ELEVEN = create("11");
    ResourceKey<JukeboxSong> WAIT = create("wait");
    ResourceKey<JukeboxSong> PIGSTEP = create("pigstep");
    ResourceKey<JukeboxSong> OTHERSIDE = create("otherside");
    ResourceKey<JukeboxSong> FIVE = create("5");
    ResourceKey<JukeboxSong> RELIC = create("relic");
    ResourceKey<JukeboxSong> PRECIPICE = create("precipice");
    ResourceKey<JukeboxSong> CREATOR = create("creator");
    ResourceKey<JukeboxSong> CREATOR_MUSIC_BOX = create("creator_music_box");
    ResourceKey<JukeboxSong> TEARS = create("tears");
    ResourceKey<JukeboxSong> LAVA_CHICKEN = create("lava_chicken");
    ResourceKey<JukeboxSong> BOUNCE = create("bounce");

    private static ResourceKey<JukeboxSong> create(String name) {
        return ResourceKey.create(Registries.JUKEBOX_SONG, Identifier.withDefaultNamespace(name));
    }
}
