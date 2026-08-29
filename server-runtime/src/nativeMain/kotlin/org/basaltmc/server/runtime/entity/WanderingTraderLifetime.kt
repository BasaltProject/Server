package org.basaltmc.server.runtime.entity

/** The explicit countdowns used instead of ordinary distance despawning by traders and their llamas. */
internal object WanderingTraderLifetime {
    fun tick(
        entity: ServerEntity,
        entities: List<ServerEntity>,
        playerIds: Set<Int>,
    ) {
        resolveLeash(entity, entities)
        when (entity.type.path) {
            "wandering_trader" -> tickTrader(entity)
            "trader_llama" -> tickLlama(entity, entities, playerIds)
        }
    }

    private fun tickTrader(trader: ServerEntity) {
        if (trader.despawnDelay > 0) {
            trader.despawnDelay--
            if (trader.despawnDelay == 0) trader.removed = true
        }
    }

    private fun tickLlama(
        llama: ServerEntity,
        entities: List<ServerEntity>,
        playerIds: Set<Int>,
    ) {
        val holder = llama.leashHolderId?.let { id -> entities.firstOrNull { it.entityId == id } }
        val leashedToTrader = holder?.type?.path == "wandering_trader"
        val leashedElsewhere = holder != null && !leashedToTrader
        val hasOnePlayerPassenger = llama.passengerIds.count { it in playerIds } == 1
        val canDespawn =
            llama.owner == null &&
                !leashedElsewhere &&
                !hasOnePlayerPassenger &&
                !llama.persistent
        if (!canDespawn) return

        llama.despawnDelay = if (leashedToTrader) holder.despawnDelay - 1 else llama.despawnDelay - 1
        if (llama.despawnDelay <= 0) {
            llama.leashHolderId = null
            llama.leashHolderUuid = null
            llama.removed = true
        }
    }

    private fun resolveLeash(
        entity: ServerEntity,
        entities: List<ServerEntity>,
    ) {
        val holderId = entity.leashHolderId
        if (holderId != null && entities.any { it.entityId == holderId }) return
        entity.leashHolderId = null
        val uuid = entity.leashHolderUuid ?: return
        entity.leashHolderId = entities.firstOrNull { it.uuid.toString() == uuid }?.entityId
        if (entity.leashHolderId == null && entity.tickCount > LEASH_RESTORE_GRACE) {
            entity.leashHolderUuid = null
            entity.sourceNbt?.remove("leash")
        }
    }

    private const val LEASH_RESTORE_GRACE = 100
}
