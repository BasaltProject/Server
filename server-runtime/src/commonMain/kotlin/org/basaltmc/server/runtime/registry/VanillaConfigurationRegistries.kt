package org.basaltmc.server.runtime.registry

import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.protocol.configuration.RegistryDataPacket
import org.basaltmc.wire.protocol.configuration.RegistryEntry

/** Static vanilla registry packets sent during client configuration. */
internal object VanillaConfigurationRegistries {
    internal fun packets(): List<RegistryDataPacket> =
        listOf(
            RegistryDataPacket(
                "minecraft:world_clock",
                VanillaRegistryData.worldClocks.map { (path, value) -> RegistryEntry("minecraft:$path", value) },
            ),
            RegistryDataPacket(
                "minecraft:dimension_type",
                VanillaRegistryData.dimensionTypes.map { (path, value) ->
                    RegistryEntry("minecraft:$path", value)
                },
            ),
            RegistryDataPacket(
                "minecraft:worldgen/biome",
                VanillaBiomes.ALL.map { entry ->
                    RegistryEntry("minecraft:${entry.path}", with(VanillaBiomes) { entry.toNbt() })
                },
            ),
            variants(
                "minecraft:banner_pattern",
                VanillaRegistryData.bannerPatterns,
                VanillaRegistryData::bannerPattern,
            ),
            variants("minecraft:wolf_variant", VanillaRegistryData.wolfVariants, VanillaRegistryData::wolfVariant),
            variants(
                "minecraft:wolf_sound_variant",
                VanillaRegistryData.wolfSoundVariants,
                VanillaRegistryData::wolfSoundVariant,
            ),
            variants("minecraft:pig_variant", VanillaRegistryData.pigVariants, VanillaRegistryData::pigVariant),
            variants(
                "minecraft:pig_sound_variant",
                VanillaRegistryData.pigSoundVariants,
                VanillaRegistryData::pigSoundVariant,
            ),
            variants("minecraft:frog_variant", VanillaRegistryData.frogVariants, VanillaRegistryData::frogVariant),
            variants("minecraft:cat_variant", VanillaRegistryData.catVariants, VanillaRegistryData::catVariant),
            variants(
                "minecraft:cat_sound_variant",
                VanillaRegistryData.catSoundVariants,
                VanillaRegistryData::catSoundVariant,
            ),
            variants(
                "minecraft:cow_sound_variant",
                VanillaRegistryData.cowSoundVariants,
                VanillaRegistryData::cowSoundVariant,
            ),
            variants("minecraft:cow_variant", VanillaRegistryData.cowVariants, VanillaRegistryData::cowVariant),
            variants(
                "minecraft:chicken_sound_variant",
                VanillaRegistryData.chickenSoundVariants,
                VanillaRegistryData::chickenSoundVariant,
            ),
            variants(
                "minecraft:chicken_variant",
                VanillaRegistryData.chickenVariants,
                VanillaRegistryData::chickenVariant,
            ),
            variants(
                "minecraft:zombie_nautilus_variant",
                VanillaRegistryData.zombieNautilusVariants,
                VanillaRegistryData::zombieNautilusVariant,
            ),
            single("minecraft:painting_variant", "humble", VanillaRegistryData.paintingVariant()),
            RegistryDataPacket(
                "minecraft:trim_material",
                VanillaRegistryData.trimMaterials.map { (id, color) ->
                    RegistryEntry("minecraft:$id", VanillaRegistryData.trimMaterial(id, color))
                },
            ),
            RegistryDataPacket(
                "minecraft:damage_type",
                VanillaRegistryData.damageTypes.map { (id, value) -> RegistryEntry("minecraft:$id", value) },
            ),
            RegistryDataPacket(
                "minecraft:jukebox_song",
                VanillaRegistryData.jukeboxSongs.map { (id, value) -> RegistryEntry("minecraft:$id", value) },
            ),
            RegistryDataPacket(
                "minecraft:instrument",
                VanillaRegistryData.instruments.map { (id, value) -> RegistryEntry("minecraft:$id", value) },
            ),
        )

    private fun variants(
        registryId: String,
        paths: List<String>,
        value: (String) -> NbtCompound,
    ): RegistryDataPacket =
        RegistryDataPacket(
            registryId,
            paths.map { path -> RegistryEntry("minecraft:$path", value(path)) },
        )

    private fun single(
        registryId: String,
        path: String,
        value: NbtCompound,
    ): RegistryDataPacket = RegistryDataPacket(registryId, listOf(RegistryEntry("minecraft:$path", value)))
}
