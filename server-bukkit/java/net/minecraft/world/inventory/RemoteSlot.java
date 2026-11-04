// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.inventory;

import net.minecraft.network.HashedPatchMap;
import net.minecraft.network.HashedStack;
import net.minecraft.world.item.ItemStack;

public interface RemoteSlot {
    RemoteSlot PLACEHOLDER = new RemoteSlot() {
        public void force(ItemStack outgoing) {}

        public void receive(HashedStack incoming) {}

        public boolean matches(ItemStack local) {
            return true;
        }
    };

    void force(ItemStack outgoing);

    void receive(HashedStack incoming);

    boolean matches(ItemStack local);

    public static class Synchronized implements RemoteSlot {
        private final HashedPatchMap.HashGenerator hasher;
        private ItemStack remoteStack;
        private HashedStack remoteHash;

        public Synchronized(HashedPatchMap.HashGenerator hasher) {
            this.hasher = hasher;
        }

        public void force(ItemStack outgoing) {
            remoteStack = outgoing.copy();
            remoteHash = null;
        }

        public void receive(HashedStack incoming) {
            remoteStack = null;
            remoteHash = incoming;
        }

        public boolean matches(ItemStack local) {
            if (remoteStack != null) {
                return ItemStack.matches(remoteStack, local);
            }
            if (remoteHash != null && remoteHash.matches(local, hasher)) {
                remoteStack = local.copy();
                return true;
            }
            return false;
        }

        public void copyFrom(Synchronized other) {
            remoteStack = other.remoteStack;
            remoteHash = other.remoteHash;
        }
    }
}
