package org.basaltmc.server.registry

import org.basaltmc.nbt.NbtCompound
import org.basaltmc.nbt.NbtInt
import org.basaltmc.nbt.NbtString
import org.basaltmc.nbt.NbtTag
import org.basaltmc.text.Component
import org.basaltmc.text.ComponentNbt
import org.basaltmc.text.TextColor

/**
 * Hand-authored NBT for the 15 registries a real 26.2 client needs during Configuration to reach
 * Play at all: `dimension_type`/`worldgen/biome` (parsed content, not just present) plus the 13
 * registries the client hard-requires non-empty (see the block below). Field shapes come from
 * decompiling the real 26.2 server jar's codecs and bootstrap data, not guessed from memory (the
 * schemas changed substantially from older, widely-known Minecraft versions). The full 29-registry
 * synced set (`damage_type`, `chat_type`, `enchantment`, and the rest) and every real entry per
 * registry (not just one representative each) is a documented gap: `basalt-datagen` generates it
 * properly later.
 */
public object VanillaRegistryData {
    /** // parity: `WorldClocks.bootstrap`; both clocks use the empty default clock codec. */
    public val worldClocks: List<Pair<String, NbtCompound>> =
        listOf(
            "overworld" to NbtCompound(),
            "the_end" to NbtCompound(),
        )

    /** // parity: `DimensionTypes.bootstrap`, in its exact registry order. */
    public val dimensionTypes: List<Pair<String, NbtCompound>> =
        listOf(
            "overworld" to overworldDimensionType(hasCeiling = false),
            "the_nether" to netherDimensionType(),
            "the_end" to endDimensionType(),
            "overworld_caves" to overworldDimensionType(hasCeiling = true),
        )

    public fun overworldDimensionType(): NbtCompound = overworldDimensionType(hasCeiling = false)

    private fun overworldDimensionType(hasCeiling: Boolean): NbtCompound =
        dimensionType(
            hasFixedTime = false,
            hasSkylight = true,
            hasCeiling = hasCeiling,
            hasEnderDragonFight = false,
            coordinateScale = 1.0,
            minY = -64,
            height = 384,
            logicalHeight = 384,
            infiniburn = "#minecraft:infiniburn_overworld",
            ambientLight = 0.0f,
            monsterSpawnLightLevel = uniformInt(0, 7),
            monsterSpawnBlockLightLimit = 0,
            attributes = overworldAttributes(),
        ).apply {
            putString("default_clock", "minecraft:overworld")
            putString("timelines", "#minecraft:in_overworld")
        }

    private fun netherDimensionType(): NbtCompound =
        dimensionType(
            hasFixedTime = true,
            hasSkylight = false,
            hasCeiling = true,
            hasEnderDragonFight = false,
            coordinateScale = 8.0,
            minY = 0,
            height = 256,
            logicalHeight = 128,
            infiniburn = "#minecraft:infiniburn_nether",
            ambientLight = 0.1f,
            monsterSpawnLightLevel = NbtInt(7),
            monsterSpawnBlockLightLimit = 15,
            attributes =
                NbtCompound().apply {
                    this["minecraft:gameplay/bed_rule"] = bedRule("never", "never", explodes = true)
                    putBoolean("minecraft:gameplay/can_start_raid", false)
                    putBoolean("minecraft:gameplay/fast_lava", true)
                    putBoolean("minecraft:gameplay/piglins_zombify", false)
                    putBoolean("minecraft:gameplay/respawn_anchor_works", true)
                    putFloat("minecraft:gameplay/sky_light_level", 4.0f)
                    putBoolean("minecraft:gameplay/snow_golem_melts", true)
                    putBoolean("minecraft:gameplay/water_evaporates", true)
                    putString("minecraft:visual/ambient_light_color", "#302821")
                    this["minecraft:visual/default_dripstone_particle"] =
                        NbtCompound().apply { putString("type", "minecraft:dripping_dripstone_lava") }
                    putFloat("minecraft:visual/fog_end_distance", 96.0f)
                    putFloat("minecraft:visual/fog_start_distance", 10.0f)
                    putString("minecraft:visual/sky_light_color", "#7a7aff")
                    putFloat("minecraft:visual/sky_light_factor", 0.0f)
                },
        ).apply {
            putString("cardinal_light", "nether")
            putString("skybox", "none")
            putString("timelines", "#minecraft:in_nether")
        }

    private fun endDimensionType(): NbtCompound =
        dimensionType(
            hasFixedTime = true,
            hasSkylight = true,
            hasCeiling = false,
            hasEnderDragonFight = true,
            coordinateScale = 1.0,
            minY = 0,
            height = 256,
            logicalHeight = 256,
            infiniburn = "#minecraft:infiniburn_end",
            ambientLight = 0.25f,
            monsterSpawnLightLevel = NbtInt(15),
            monsterSpawnBlockLightLimit = 0,
            attributes =
                NbtCompound().apply {
                    this["minecraft:audio/ambient_sounds"] = ambientCaveSounds()
                    this["minecraft:audio/background_music"] =
                        NbtCompound().apply {
                            this["default"] = music("minecraft:music.end", 6000, 24000, replace = true)
                        }
                    this["minecraft:gameplay/bed_rule"] = bedRule("never", "never", explodes = true)
                    putBoolean("minecraft:gameplay/respawn_anchor_works", false)
                    putString("minecraft:visual/ambient_light_color", "#3f473f")
                    putString("minecraft:visual/fog_color", "#181318")
                    putString("minecraft:visual/sky_color", "#000000")
                    putString("minecraft:visual/sky_light_color", "#ac60cd")
                    putFloat("minecraft:visual/sky_light_factor", 0.0f)
                },
        ).apply {
            putString("default_clock", "minecraft:the_end")
            putString("skybox", "end")
            putString("timelines", "#minecraft:in_end")
        }

    private fun dimensionType(
        hasFixedTime: Boolean,
        hasSkylight: Boolean,
        hasCeiling: Boolean,
        hasEnderDragonFight: Boolean,
        coordinateScale: Double,
        minY: Int,
        height: Int,
        logicalHeight: Int,
        infiniburn: String,
        ambientLight: Float,
        monsterSpawnLightLevel: NbtTag,
        monsterSpawnBlockLightLimit: Int,
        attributes: NbtCompound,
    ): NbtCompound =
        NbtCompound().apply {
            if (hasFixedTime) putBoolean("has_fixed_time", true)
            putBoolean("has_skylight", hasSkylight)
            putBoolean("has_ceiling", hasCeiling)
            putBoolean("has_ender_dragon_fight", hasEnderDragonFight)
            putDouble("coordinate_scale", coordinateScale)
            putInt("min_y", minY)
            putInt("height", height)
            putInt("logical_height", logicalHeight)
            this["infiniburn"] = NbtString(infiniburn)
            putFloat("ambient_light", ambientLight)
            this["monster_spawn_light_level"] = monsterSpawnLightLevel
            putInt("monster_spawn_block_light_limit", monsterSpawnBlockLightLimit)
            this["attributes"] = attributes
        }

    private fun overworldAttributes(): NbtCompound =
        NbtCompound().apply {
            this["minecraft:audio/ambient_sounds"] = ambientCaveSounds()
            this["minecraft:audio/background_music"] =
                NbtCompound().apply {
                    this["creative"] = music("minecraft:music.creative", 12000, 24000)
                    this["default"] = music("minecraft:music.game", 12000, 24000)
                }
            this["minecraft:gameplay/bed_rule"] =
                bedRule("always", "when_dark", explodes = false).apply {
                    this["error_message"] =
                        NbtCompound().apply { putString("translate", "block.minecraft.bed.no_sleep") }
                }
            putBoolean("minecraft:gameplay/nether_portal_spawns_piglin", true)
            putBoolean("minecraft:gameplay/respawn_anchor_works", false)
            putString("minecraft:visual/ambient_light_color", "#0a0a0a")
            putString("minecraft:visual/cloud_color", "#ccffffff")
            putFloat("minecraft:visual/cloud_height", 192.33f)
            putString("minecraft:visual/fog_color", "#c0d8ff")
            putString("minecraft:visual/sky_color", "#78a7ff")
        }

    private fun ambientCaveSounds(): NbtCompound =
        NbtCompound().apply {
            this["mood"] =
                NbtCompound().apply {
                    putInt("block_search_extent", 8)
                    putDouble("offset", 2.0)
                    putString("sound", "minecraft:ambient.cave")
                    putInt("tick_delay", 6000)
                }
        }

    private fun music(
        sound: String,
        minDelay: Int,
        maxDelay: Int,
        replace: Boolean = false,
    ): NbtCompound =
        NbtCompound().apply {
            putInt("max_delay", maxDelay)
            putInt("min_delay", minDelay)
            if (replace) putBoolean("replace_current_music", true)
            putString("sound", sound)
        }

    private fun bedRule(
        canSetSpawn: String,
        canSleep: String,
        explodes: Boolean,
    ): NbtCompound =
        NbtCompound().apply {
            putString("can_set_spawn", canSetSpawn)
            putString("can_sleep", canSleep)
            if (explodes) putBoolean("explodes", true)
        }

    private fun uniformInt(
        min: Int,
        max: Int,
    ): NbtCompound =
        NbtCompound().apply {
            putString("type", "minecraft:uniform")
            putInt("max_inclusive", max)
            putInt("min_inclusive", min)
        }

    // The biome registry (`minecraft:worldgen/biome`) is synced in full by [VanillaBiomes]: all 66
    // vanilla entries, not a single plains stand-in.

    // The 13 registries below are the only ones a real 26.2 client hard-requires non-empty to
    // finish Configuration (confirmed live: RegistryDataLoader.SYNCHRONIZED_REGISTRIES marks
    // exactly these with RegistryValidator.nonEmpty(), and no others). Their NETWORK_CODEC (used
    // for network sync, not the full DIRECT_CODEC used for the datapack file) drops
    // `spawn_conditions` entirely for the animal variants, so only asset references are needed.
    // Field values are the real vanilla ones (one representative entry per registry, not the
    // full set): full registry parity across all 29 synced registries is `basalt-datagen`'s job.

    public fun wolfVariant(): NbtCompound {
        fun assets(
            wild: String,
            tame: String,
            angry: String,
        ) = NbtCompound().apply {
            putString("wild", wild)
            putString("tame", tame)
            putString("angry", angry)
        }
        return NbtCompound().apply {
            this["assets"] =
                assets(
                    "minecraft:entity/wolf/wolf_black",
                    "minecraft:entity/wolf/wolf_black_tame",
                    "minecraft:entity/wolf/wolf_black_angry",
                )
            this["baby_assets"] =
                assets(
                    "minecraft:entity/wolf/wolf_black_baby",
                    "minecraft:entity/wolf/wolf_black_tame_baby",
                    "minecraft:entity/wolf/wolf_black_angry_baby",
                )
        }
    }

    public fun wolfSoundVariant(): NbtCompound {
        fun sounds(prefix: String) =
            NbtCompound().apply {
                putString("ambient_sound", "minecraft:entity.$prefix.ambient")
                putString("death_sound", "minecraft:entity.$prefix.death")
                putString("growl_sound", "minecraft:entity.$prefix.growl")
                putString("hurt_sound", "minecraft:entity.$prefix.hurt")
                putString("pant_sound", "minecraft:entity.$prefix.pant")
                putString("whine_sound", "minecraft:entity.$prefix.whine")
                putString("step_sound", "minecraft:entity.$prefix.step")
            }
        return NbtCompound().apply {
            this["adult_sounds"] = sounds("wolf")
            this["baby_sounds"] = sounds("baby_wolf")
        }
    }

    public fun pigVariant(): NbtCompound =
        NbtCompound().apply {
            putString("asset_id", "minecraft:entity/pig/pig_temperate")
            putString("baby_asset_id", "minecraft:entity/pig/pig_temperate_baby")
        }

    public fun pigSoundVariant(): NbtCompound {
        fun sounds(prefix: String) =
            NbtCompound().apply {
                putString("ambient_sound", "minecraft:entity.$prefix.ambient")
                putString("death_sound", "minecraft:entity.$prefix.death")
                putString("eat_sound", "minecraft:entity.$prefix.eat")
                putString("hurt_sound", "minecraft:entity.$prefix.hurt")
                putString("step_sound", "minecraft:entity.$prefix.step")
            }
        return NbtCompound().apply {
            this["adult_sounds"] = sounds("pig")
            this["baby_sounds"] = sounds("baby_pig")
        }
    }

    public fun frogVariant(): NbtCompound =
        NbtCompound().apply { putString("asset_id", "minecraft:entity/frog/frog_cold") }

    public fun catVariant(): NbtCompound =
        NbtCompound().apply {
            putString("asset_id", "minecraft:entity/cat/cat_black")
            putString("baby_asset_id", "minecraft:entity/cat/cat_black_baby")
        }

    public fun catSoundVariant(): NbtCompound {
        fun sounds(prefix: String) =
            NbtCompound().apply {
                putString("ambient_sound", "minecraft:entity.$prefix.ambient")
                putString("beg_for_food_sound", "minecraft:entity.$prefix.beg_for_food")
                putString("death_sound", "minecraft:entity.$prefix.death")
                putString("eat_sound", "minecraft:entity.$prefix.eat")
                putString("hiss_sound", "minecraft:entity.$prefix.hiss")
                putString("hurt_sound", "minecraft:entity.$prefix.hurt")
                putString("purr_sound", "minecraft:entity.$prefix.purr")
                putString("purreow_sound", "minecraft:entity.$prefix.purreow")
                putString("stray_ambient_sound", "minecraft:entity.$prefix.stray_ambient")
            }
        return NbtCompound().apply {
            this["adult_sounds"] = sounds("cat")
            this["baby_sounds"] = sounds("baby_cat")
        }
    }

    public fun cowSoundVariant(): NbtCompound =
        NbtCompound().apply {
            putString("ambient_sound", "minecraft:entity.cow.ambient")
            putString("death_sound", "minecraft:entity.cow.death")
            putString("hurt_sound", "minecraft:entity.cow.hurt")
            putString("step_sound", "minecraft:entity.cow.step")
        }

    public fun cowVariant(): NbtCompound =
        NbtCompound().apply {
            putString("asset_id", "minecraft:entity/cow/cow_temperate")
            putString("baby_asset_id", "minecraft:entity/cow/cow_temperate_baby")
        }

    public fun chickenSoundVariant(): NbtCompound {
        fun sounds(prefix: String) =
            NbtCompound().apply {
                putString("ambient_sound", "minecraft:entity.$prefix.ambient")
                putString("death_sound", "minecraft:entity.$prefix.death")
                putString("hurt_sound", "minecraft:entity.$prefix.hurt")
                putString("step_sound", "minecraft:entity.$prefix.step")
            }
        return NbtCompound().apply {
            this["adult_sounds"] = sounds("chicken")
            this["baby_sounds"] = sounds("baby_chicken")
        }
    }

    // Unlike the other animal variants, chicken_variant needs three entries, not just DEFAULT: the
    // egg item, blue_egg, and brown_egg items each carry a default component resolving a distinct
    // ChickenVariants key (temperate, cold, warm respectively) directly, confirmed live.
    public fun chickenVariant(id: String): NbtCompound =
        NbtCompound().apply {
            putString("asset_id", "minecraft:entity/chicken/chicken_$id")
            putString("baby_asset_id", "minecraft:entity/chicken/chicken_${id}_baby")
        }

    public fun zombieNautilusVariant(): NbtCompound =
        NbtCompound().apply { putString("asset_id", "minecraft:entity/nautilus/zombie_nautilus_coral") }

    public fun paintingVariant(): NbtCompound =
        NbtCompound().apply {
            putInt("width", 2)
            putInt("height", 2)
            putString("asset_id", "minecraft:humble")
        }

    // trim_material isn't in the 13 "must be non-empty" set, but several base items (redstone dust,
    // ingots, gems, resin brick: see Items.java's `.trimMaterial(...)` calls) carry a default
    // `ProvidesTrimMaterial` component that resolves a specific entry by ResourceKey at Configuration
    // end, independent of the registry's own emptiness. All eleven entries vanilla items reference
    // are needed, not just one representative. Field shapes from TrimMaterial.DIRECT_CODEC
    // (TrimMaterial has no NETWORK_CODEC, so the full codec is used for sync): `asset_name` and
    // `description` flattened at the top level (MaterialAssetGroup.MAP_CODEC), `override_armor_assets`
    // omitted since it defaults to empty.
    public fun trimMaterial(
        id: String,
        colorRgb: Int,
    ): NbtCompound =
        NbtCompound().apply {
            putString("asset_name", id)
            this["description"] =
                ComponentNbt.serialize(Component.translatable("trim_material.minecraft.$id").color(TextColor(colorRgb)))
        }

    public val trimMaterials: List<Pair<String, Int>> =
        listOf(
            "quartz" to 14931140,
            "iron" to 15527148,
            "netherite" to 6445145,
            "redstone" to 9901575,
            "copper" to 11823181,
            "gold" to 14594349,
            "emerald" to 1155126,
            "diamond" to 7269586,
            "lapis" to 4288151,
            "amethyst" to 10116294,
            "resin" to 16545810,
        )

    // damage_type isn't in the 13 "must be non-empty" set either, but weapons built via
    // Item.Properties.spear() (tridents) carry a default DAMAGE_TYPE component that resolves
    // `minecraft:damage_type/minecraft:spear` by ResourceKey: confirmed live, a real client failed
    // there right after trim_material was fixed. Sending the full real registry (all entries from
    // DamageTypes.bootstrap, values taken verbatim from the jar's embedded
    // `data/minecraft/damage_type/*.json`) rather than just `spear`, since every entry is equally
    // cheap to include and this is likely to matter again for combat/death-message text later.
    private fun damageType(
        messageId: String,
        scaling: String = "when_caused_by_living_non_player",
        exhaustion: Float,
        effects: String? = null,
        deathMessageType: String? = null,
    ): NbtCompound =
        NbtCompound().apply {
            putString("message_id", messageId)
            putString("scaling", scaling)
            putFloat("exhaustion", exhaustion)
            effects?.let { putString("effects", it) }
            deathMessageType?.let { putString("death_message_type", it) }
        }

    public val damageTypes: List<Pair<String, NbtCompound>> =
        listOf(
            "in_fire" to damageType("inFire", exhaustion = 0.1f, effects = "burning"),
            "campfire" to damageType("inFire", exhaustion = 0.1f, effects = "burning"),
            "lightning_bolt" to damageType("lightningBolt", exhaustion = 0.1f),
            "on_fire" to damageType("onFire", exhaustion = 0.0f, effects = "burning"),
            "lava" to damageType("lava", exhaustion = 0.1f, effects = "burning"),
            "hot_floor" to damageType("hotFloor", exhaustion = 0.1f, effects = "burning"),
            "sulfur_cube_hot" to damageType("sulfurCubeHot", exhaustion = 0.1f, effects = "burning"),
            "in_wall" to damageType("inWall", exhaustion = 0.0f),
            "cramming" to damageType("cramming", exhaustion = 0.0f),
            "drown" to damageType("drown", exhaustion = 0.0f, effects = "drowning"),
            "starve" to damageType("starve", exhaustion = 0.0f),
            "cactus" to damageType("cactus", exhaustion = 0.1f),
            "fall" to damageType("fall", exhaustion = 0.0f, deathMessageType = "fall_variants"),
            "ender_pearl" to damageType("fall", exhaustion = 0.0f, deathMessageType = "fall_variants"),
            "fly_into_wall" to damageType("flyIntoWall", exhaustion = 0.0f),
            "out_of_world" to damageType("outOfWorld", exhaustion = 0.0f),
            "generic" to damageType("generic", exhaustion = 0.0f),
            "magic" to damageType("magic", exhaustion = 0.0f),
            "wither" to damageType("wither", exhaustion = 0.0f),
            "dragon_breath" to damageType("dragonBreath", exhaustion = 0.0f),
            "dry_out" to damageType("dryout", exhaustion = 0.1f),
            "sweet_berry_bush" to damageType("sweetBerryBush", exhaustion = 0.1f, effects = "poking"),
            "freeze" to damageType("freeze", exhaustion = 0.0f, effects = "freezing"),
            "stalagmite" to damageType("stalagmite", exhaustion = 0.0f),
            "falling_block" to damageType("fallingBlock", exhaustion = 0.1f),
            "falling_anvil" to damageType("anvil", exhaustion = 0.1f),
            "falling_stalactite" to damageType("fallingStalactite", exhaustion = 0.1f),
            "sting" to damageType("sting", exhaustion = 0.1f),
            "mob_attack" to damageType("mob", exhaustion = 0.1f),
            "mob_attack_no_aggro" to damageType("mob", exhaustion = 0.1f),
            "player_attack" to damageType("player", exhaustion = 0.1f),
            "spear" to damageType("spear", exhaustion = 0.1f),
            "arrow" to damageType("arrow", exhaustion = 0.1f),
            "trident" to damageType("trident", exhaustion = 0.1f),
            "mob_projectile" to damageType("mob", exhaustion = 0.1f),
            "spit" to damageType("mob", exhaustion = 0.1f),
            "fireworks" to damageType("fireworks", exhaustion = 0.1f),
            "unattributed_fireball" to damageType("onFire", exhaustion = 0.1f, effects = "burning"),
            "fireball" to damageType("fireball", exhaustion = 0.1f, effects = "burning"),
            "wither_skull" to damageType("witherSkull", exhaustion = 0.1f),
            "thrown" to damageType("thrown", exhaustion = 0.1f),
            "indirect_magic" to damageType("indirectMagic", exhaustion = 0.0f),
            "thorns" to damageType("thorns", exhaustion = 0.1f, effects = "thorns"),
            "explosion" to damageType("explosion", scaling = "always", exhaustion = 0.1f),
            "player_explosion" to damageType("explosion.player", scaling = "always", exhaustion = 0.1f),
            "sonic_boom" to damageType("sonic_boom", scaling = "always", exhaustion = 0.0f),
            "bad_respawn_point" to
                damageType(
                    "badRespawnPoint",
                    scaling = "always",
                    exhaustion = 0.1f,
                    deathMessageType = "intentional_game_design",
                ),
            "outside_border" to damageType("outsideBorder", exhaustion = 0.0f),
            "generic_kill" to damageType("genericKill", exhaustion = 0.0f),
            "wind_charge" to damageType("mob", exhaustion = 0.1f),
            "mace_smash" to damageType("mace_smash", exhaustion = 0.1f),
        )

    // jukebox_song isn't in the 13 either, but every music disc item (Items.java's
    // `.jukeboxPlayable(JukeboxSongs.*)` calls) carries a default component resolving its song by
    // ResourceKey. Values taken verbatim from the jar's embedded `data/minecraft/jukebox_song/*.json`.
    private fun jukeboxSong(
        soundEvent: String,
        translateKey: String,
        lengthInSeconds: Float,
        comparatorOutput: Int,
    ): NbtCompound =
        NbtCompound().apply {
            putString("sound_event", soundEvent)
            this["description"] = ComponentNbt.serialize(Component.translatable(translateKey))
            putFloat("length_in_seconds", lengthInSeconds)
            putInt("comparator_output", comparatorOutput)
        }

    public val jukeboxSongs: List<Pair<String, NbtCompound>> =
        listOf(
            "13" to jukeboxSong("minecraft:music_disc.13", "jukebox_song.minecraft.13", 178.0f, 1),
            "cat" to jukeboxSong("minecraft:music_disc.cat", "jukebox_song.minecraft.cat", 185.0f, 2),
            "blocks" to jukeboxSong("minecraft:music_disc.blocks", "jukebox_song.minecraft.blocks", 345.0f, 3),
            "bounce" to jukeboxSong("minecraft:music_disc.bounce", "jukebox_song.minecraft.bounce", 234.0f, 8),
            "chirp" to jukeboxSong("minecraft:music_disc.chirp", "jukebox_song.minecraft.chirp", 185.0f, 4),
            "creator" to jukeboxSong("minecraft:music_disc.creator", "jukebox_song.minecraft.creator", 176.0f, 12),
            "creator_music_box" to
                jukeboxSong(
                    "minecraft:music_disc.creator_music_box",
                    "jukebox_song.minecraft.creator_music_box",
                    73.0f,
                    11,
                ),
            "far" to jukeboxSong("minecraft:music_disc.far", "jukebox_song.minecraft.far", 174.0f, 5),
            "lava_chicken" to
                jukeboxSong("minecraft:music_disc.lava_chicken", "jukebox_song.minecraft.lava_chicken", 134.0f, 9),
            "mall" to jukeboxSong("minecraft:music_disc.mall", "jukebox_song.minecraft.mall", 197.0f, 6),
            "mellohi" to jukeboxSong("minecraft:music_disc.mellohi", "jukebox_song.minecraft.mellohi", 96.0f, 7),
            "stal" to jukeboxSong("minecraft:music_disc.stal", "jukebox_song.minecraft.stal", 150.0f, 8),
            "strad" to jukeboxSong("minecraft:music_disc.strad", "jukebox_song.minecraft.strad", 188.0f, 9),
            "ward" to jukeboxSong("minecraft:music_disc.ward", "jukebox_song.minecraft.ward", 251.0f, 10),
            "11" to jukeboxSong("minecraft:music_disc.11", "jukebox_song.minecraft.11", 71.0f, 11),
            "wait" to jukeboxSong("minecraft:music_disc.wait", "jukebox_song.minecraft.wait", 238.0f, 12),
            "otherside" to
                jukeboxSong(
                    "minecraft:music_disc.otherside",
                    "jukebox_song.minecraft.otherside",
                    195.0f,
                    14,
                ),
            "relic" to jukeboxSong("minecraft:music_disc.relic", "jukebox_song.minecraft.relic", 218.0f, 14),
            "5" to jukeboxSong("minecraft:music_disc.5", "jukebox_song.minecraft.5", 178.0f, 15),
            "pigstep" to jukeboxSong("minecraft:music_disc.pigstep", "jukebox_song.minecraft.pigstep", 149.0f, 13),
            "precipice" to
                jukeboxSong(
                    "minecraft:music_disc.precipice",
                    "jukebox_song.minecraft.precipice",
                    299.0f,
                    13,
                ),
            "tears" to jukeboxSong("minecraft:music_disc.tears", "jukebox_song.minecraft.tears", 175.0f, 10),
        )

    // instrument isn't in the 13 either, but Items.GOAT_HORN carries a default INSTRUMENT component
    // resolving `minecraft:instrument/minecraft:ponder_goat_horn` by ResourceKey. All eight goat horn
    // sounds included for completeness (they differ only by sound_event index); values verbatim from
    // `data/minecraft/instrument/*.json`.
    private fun instrument(
        soundEvent: String,
        translateKey: String,
    ): NbtCompound =
        NbtCompound().apply {
            putString("sound_event", soundEvent)
            putFloat("use_duration", 7.0f)
            putFloat("range", 256.0f)
            this["description"] = ComponentNbt.serialize(Component.translatable(translateKey))
        }

    public val instruments: List<Pair<String, NbtCompound>> =
        listOf(
            "ponder_goat_horn" to
                instrument("minecraft:item.goat_horn.sound.0", "instrument.minecraft.ponder_goat_horn"),
            "sing_goat_horn" to instrument("minecraft:item.goat_horn.sound.1", "instrument.minecraft.sing_goat_horn"),
            "seek_goat_horn" to instrument("minecraft:item.goat_horn.sound.2", "instrument.minecraft.seek_goat_horn"),
            "feel_goat_horn" to instrument("minecraft:item.goat_horn.sound.3", "instrument.minecraft.feel_goat_horn"),
            "admire_goat_horn" to
                instrument("minecraft:item.goat_horn.sound.4", "instrument.minecraft.admire_goat_horn"),
            "call_goat_horn" to instrument("minecraft:item.goat_horn.sound.5", "instrument.minecraft.call_goat_horn"),
            "yearn_goat_horn" to instrument("minecraft:item.goat_horn.sound.6", "instrument.minecraft.yearn_goat_horn"),
            "dream_goat_horn" to instrument("minecraft:item.goat_horn.sound.7", "instrument.minecraft.dream_goat_horn"),
        )
}
