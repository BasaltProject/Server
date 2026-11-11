// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world;
import net.minecraft.world.entity.ContainerUser;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
public class CompoundContainer implements Container {
    private final Container first;
    private final Container second;
    public CompoundContainer(Container container1, Container container2) {
        this.first = container1;
        this.second = container2;
    }
    private boolean inSecond(int slot) {
        return slot >= first.getContainerSize();
    }
    private int local(int slot) {
        return inSecond(slot) ? slot - first.getContainerSize() : slot;
    }
    private Container half(int slot) {
        return inSecond(slot) ? second : first;
    }
    public int getContainerSize() {
        return first.getContainerSize() + second.getContainerSize();
    }
    public boolean isEmpty() {
        return first.isEmpty() && second.isEmpty();
    }
    public boolean contains(Container container) {
        return first == container || second == container;
    }
    public ItemStack getItem(int slot) {
        return half(slot).getItem(local(slot));
    }
    public ItemStack removeItem(int slot, int count) {
        return half(slot).removeItem(local(slot), count);
    }
    public ItemStack removeItemNoUpdate(int slot) {
        return half(slot).removeItemNoUpdate(local(slot));
    }
    public void setItem(int slot, ItemStack itemStack) {
        half(slot).setItem(local(slot), itemStack);
    }
    public int getMaxStackSize() {
        return first.getMaxStackSize();
    }
    public void setChanged() {
        first.setChanged();
        second.setChanged();
    }
    public boolean stillValid(Player player) {
        return first.stillValid(player) && second.stillValid(player);
    }
    public void startOpen(ContainerUser containerUser) {
        first.startOpen(containerUser);
        second.startOpen(containerUser);
    }
    public void stopOpen(ContainerUser containerUser) {
        first.stopOpen(containerUser);
        second.stopOpen(containerUser);
    }
    public boolean canPlaceItem(int slot, ItemStack itemStack) {
        return half(slot).canPlaceItem(local(slot), itemStack);
    }
    public void clearContent() {
        first.clearContent();
        second.clearContent();
    }
}
