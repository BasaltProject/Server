package org.basaltmc.server.runtime.entity

/**
 * Mobs vanilla excludes from natural caps and despawning without setting PersistenceRequired.
 *
 * parity: `Mob.requiresCustomPersistence` plus the overrides present in Minecraft 26.2. Runtime
 * fields are preferred; preserved source NBT covers state Basalt does not yet model directly.
 */
public object MobPersistence {
    /** `isPersistenceRequired || requiresCustomPersistence`. */
    public fun keeps(entity: ServerEntity): Boolean = entity.persistent || requiresCustomPersistence(entity)

    public fun requiresCustomPersistence(entity: ServerEntity): Boolean {
        // Base Mob: a rider and a leashed mob cannot vanish out from under their relationship.
        if (entity.vehicleId != null) return true
        if (entity.leashHolderId != null || entity.leashHolderUuid != null) return true
        val data = entity.sourceNbt
        if (data != null && ("leash" in data || "Leash" in data)) return true

        // Taming calls setPersistenceRequired in vanilla. Owner is the equivalent live Basalt state.
        if (entity.owner != null) return true
        if (entity.raidCenter != null) return true

        return when (entity.type.path) {
            in FROM_BUCKET_TYPES -> data?.getBoolean("FromBucket") == true
            "chicken" -> data?.getBoolean("IsChickenJockey") == true
            "enderman" -> data?.getCompound("carriedBlockState")?.size != 0
            "sulfur_cube" ->
                data?.getBoolean("from_bucket") == true ||
                    hasBodyItem(data)
            else -> false
        }
    }

    private fun hasBodyItem(data: org.basaltmc.wire.nbt.NbtCompound?): Boolean {
        val body = data?.getCompound("equipment")?.getCompound("body") ?: return false
        return body.getString("id").isNotEmpty() && body.getInt("count") > 0
    }

    /** AbstractFish (including tadpoles) and Axolotl's exact bucket-persistence key. */
    private val FROM_BUCKET_TYPES = setOf("axolotl", "cod", "pufferfish", "salmon", "tadpole", "tropical_fish")
}
