package net.minecraft.core;

import net.minecraft.tags.TagKey;
import org.basaltmc.bukkit.Unsupported;
public interface HolderSet<T> {

    public abstract static class ListBacked<T> implements HolderSet {
        protected ListBacked() {
        }
    }

    public static class Named<T> extends HolderSet.ListBacked {
        protected Named() {
        }
        public TagKey key() {
            throw Unsupported.hit("net.minecraft.core.HolderSet.Named.key");
        }
    }
}
