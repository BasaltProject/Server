package net.minecraft.core;

import com.mojang.serialization.DynamicOps;
import net.minecraft.resources.RegistryOps;
import org.basaltmc.bukkit.Unsupported;
public interface HolderLookup<T> extends HolderGetter {

    public interface Provider extends HolderGetter.Provider {
        public default RegistryOps createSerializationContext(DynamicOps parent) {
            throw Unsupported.hit("net.minecraft.core.HolderLookup.Provider.createSerializationContext");
        }
    }

    public interface RegistryLookup<T> extends HolderLookup {
    }
}
