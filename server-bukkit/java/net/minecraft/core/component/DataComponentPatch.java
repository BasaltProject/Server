package net.minecraft.core.component;

import com.mojang.serialization.Codec;
import com.mojang.serialization.Dynamic;
import com.mojang.serialization.JavaOps;

public final class DataComponentPatch {
    public static final DataComponentPatch EMPTY = new DataComponentPatch(null);
    public static final Codec<DataComponentPatch> CODEC = Codec.PASSTHROUGH.xmap(
        data -> new DataComponentPatch(data),
        patch -> patch.data != null ? patch.data : new Dynamic<>(JavaOps.INSTANCE, JavaOps.INSTANCE.emptyMap()));

    private final Dynamic<?> data;

    private DataComponentPatch(Dynamic<?> data) {
        this.data = data;
    }

    public boolean isEmpty() {
        return data == null;
    }

    public Dynamic<?> data() {
        return data;
    }
}
