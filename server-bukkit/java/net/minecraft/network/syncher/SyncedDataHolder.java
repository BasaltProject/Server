// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.syncher;

import java.util.List;
public interface SyncedDataHolder {
    default void onSyncedDataUpdated(EntityDataAccessor<?> accessor) {
    }
    default void onSyncedDataUpdated(List<SynchedEntityData.DataValue<?>> updatedItems) {
    }
}
