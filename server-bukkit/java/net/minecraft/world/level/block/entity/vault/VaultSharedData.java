// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity.vault;

import com.mojang.serialization.Codec;
import java.util.LinkedHashSet;
import java.util.Set;
import java.util.UUID;
import java.util.stream.Collectors;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.item.ItemStack;

public class VaultSharedData {
    public static final String TAG_NAME = "shared_data";
    public static final Codec<VaultSharedData> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.block.entity.vault.VaultSharedData.class, "CODEC");

    private ItemStack display = ItemStack.EMPTY;
    private Set<UUID> connected = new LinkedHashSet<>();
    private double particleRange = VaultConfig.DEFAULT.deactivationRange();
    boolean isDirty;

    public VaultSharedData(ItemStack displayItem, Set<UUID> connectedPlayers, double connectedParticlesRange) {
        this.display = displayItem;
        this.connected.addAll(connectedPlayers);
        this.particleRange = connectedParticlesRange;
    }

    public VaultSharedData() {}

    public ItemStack getDisplayItem() {
        return display;
    }

    public boolean hasDisplayItem() {
        return !display.isEmpty();
    }

    public void setDisplayItem(ItemStack stack) {
        if (ItemStack.matches(display, stack)) {
            return;
        }
        display = stack.copy();
        isDirty = true;
    }

    boolean hasConnectedPlayers() {
        return !connected.isEmpty();
    }

    Set<UUID> getConnectedPlayers() {
        return connected;
    }

    double connectedParticlesRange() {
        return particleRange;
    }

    void updateConnectedPlayersWithinRange(ServerLevel serverLevel, BlockPos pos, VaultServerData serverData, VaultConfig config, double limit) {
        Set<UUID> nearby = config.playerDetector().detect(serverLevel, config.entitySelector(), pos, limit, false).stream()
            .filter(id -> !serverData.getRewardedPlayers().contains(id)).collect(Collectors.toSet());
        if (!connected.equals(nearby)) {
            connected = nearby;
            isDirty = true;
        }
    }

    void set(VaultSharedData from) {
        display = from.display;
        connected = from.connected;
        particleRange = from.particleRange;
    }
}
