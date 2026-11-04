// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot;

import java.util.HashMap;
import java.util.Map;
import java.util.function.Consumer;
import net.minecraft.resources.Identifier;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.context.ContextKey;
import net.minecraft.util.context.ContextKeySet;
import net.minecraft.util.context.ContextMap;
import net.minecraft.world.item.ItemStack;

public class LootParams {
    private final ServerLevel level;
    private final ContextMap params;
    private final Map<Identifier, DynamicDrop> dynamicDrops;
    private final float luck;

    public LootParams(ServerLevel level, ContextMap params, Map<Identifier, DynamicDrop> dynamicDrops, float luck) {
        this.level = level;
        this.params = params;
        this.dynamicDrops = dynamicDrops;
        this.luck = luck;
    }

    public ServerLevel getLevel() {
        return level;
    }

    public ContextMap contextMap() {
        return params;
    }

    public void addDynamicDrops(Identifier location, Consumer<ItemStack> output) {
        DynamicDrop drop = dynamicDrops.get(location);
        if (drop != null) {
            drop.add(output);
        }
    }

    public float getLuck() {
        return luck;
    }

    public static class Builder {
        private final ServerLevel level;
        private final ContextMap.Builder params = ContextMap.builder();
        private final Map<Identifier, DynamicDrop> dynamicDrops = new HashMap<>();
        private float luck;

        public Builder(ServerLevel level) {
            this.level = level;
        }

        public ServerLevel getLevel() {
            return level;
        }

        public ContextMap contextMap() {
            return params.build();
        }

        public <T> Builder withParameter(ContextKey<T> param, T value) {
            params.set(param, value);
            return this;
        }

        public <T> Builder withOptionalParameter(ContextKey<T> param, T value) {
            if (value != null) {
                params.set(param, value);
            }
            return this;
        }

        public <T> T getParameter(ContextKey<T> param) {
            T value = params.get(param);
            if (value == null) {
                throw new IllegalArgumentException("No parameter " + param);
            }
            return value;
        }

        public <T> T getOptionalParameter(ContextKey<T> param) {
            return params.get(param);
        }

        public Builder withDynamicDrop(Identifier location, DynamicDrop dynamicDrop) {
            if (dynamicDrops.put(location, dynamicDrop) != null) {
                throw new IllegalStateException("Duplicated dynamic drop '" + location + "'");
            }
            return this;
        }

        public Builder withLuck(float luck) {
            this.luck = luck;
            return this;
        }

        public LootParams create(ContextKeySet contextKeySet) {
            return new LootParams(level, params.buildAndValidate(contextKeySet), dynamicDrops, luck);
        }
    }

    @FunctionalInterface
    public interface DynamicDrop {
        void add(Consumer<ItemStack> output);
    }
}
