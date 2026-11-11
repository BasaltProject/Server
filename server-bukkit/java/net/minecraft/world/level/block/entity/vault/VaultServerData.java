// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity.vault;

import com.mojang.serialization.Codec;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;
import java.util.UUID;
import net.minecraft.util.Mth;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;

public class VaultServerData {
    public static final String TAG_NAME = "server_data";
    public static final Codec<VaultServerData> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.block.entity.vault.VaultServerData.class, "CODEC");
    private static final int kMaxRewarded = 128;

    private final Set<UUID> rewarded = new LinkedHashSet<>();
    private final List<ItemStack> pending = new ArrayList<>();
    private long resumeAt;
    private long lastInsertFail;
    private int ejectionsNeeded;
    boolean isDirty;

    public VaultServerData(Set<UUID> rewardedPlayers, long stateUpdatingResumesAt, List<ItemStack> itemsToEject, int totalEjectionsNeeded) {
        rewarded.addAll(rewardedPlayers);
        resumeAt = stateUpdatingResumesAt;
        pending.addAll(itemsToEject);
        ejectionsNeeded = totalEjectionsNeeded;
    }

    public VaultServerData() {}

    void setLastInsertFailTimestamp(long lastInsertFailTimestamp) {
        lastInsertFail = lastInsertFailTimestamp;
    }

    long getLastInsertFailTimestamp() {
        return lastInsertFail;
    }

    Set<UUID> getRewardedPlayers() {
        return rewarded;
    }

    boolean hasRewardedPlayer(Player player) {
        return rewarded.contains(player.getUUID());
    }

    public void addToRewardedPlayers(Player player) {
        rewarded.add(player.getUUID());
        if (rewarded.size() > kMaxRewarded) {
            Iterator<UUID> oldest = rewarded.iterator();
            oldest.next();
            oldest.remove();
        }
        isDirty = true;
    }

    long stateUpdatingResumesAt() {
        return resumeAt;
    }

    void pauseStateUpdatingUntil(long stateUpdatingResumesAt) {
        resumeAt = stateUpdatingResumesAt;
        isDirty = true;
    }

    List<ItemStack> getItemsToEject() {
        return pending;
    }

    void markEjectionFinished() {
        ejectionsNeeded = 0;
        isDirty = true;
    }

    void setItemsToEject(List<ItemStack> newItemsToEject) {
        pending.clear();
        pending.addAll(newItemsToEject);
        ejectionsNeeded = pending.size();
        isDirty = true;
    }

    ItemStack getNextItemToEject() {
        return pending.isEmpty() ? ItemStack.EMPTY : pending.get(pending.size() - 1);
    }

    ItemStack popNextItemToEject() {
        if (pending.isEmpty()) {
            return ItemStack.EMPTY;
        }
        isDirty = true;
        return pending.remove(pending.size() - 1);
    }

    void set(VaultServerData from) {
        resumeAt = from.resumeAt;
        pending.clear();
        pending.addAll(from.pending);
        rewarded.clear();
        rewarded.addAll(from.rewarded);
    }

    public float ejectionProgress() {
        return ejectionsNeeded == 1 ? 1.0F : 1.0F - Mth.inverseLerp((float) pending.size(), 1.0F, (float) ejectionsNeeded);
    }
}
