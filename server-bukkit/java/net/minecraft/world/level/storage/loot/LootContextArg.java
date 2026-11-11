// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot;
import com.mojang.serialization.Codec;
import java.util.function.Function;
import java.util.function.UnaryOperator;
import net.minecraft.util.StringRepresentable;
import net.minecraft.util.context.ContextKey;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.item.ItemInstance;
import net.minecraft.world.level.block.entity.BlockEntity;
public interface LootContextArg<R> {
    Codec<LootContextArg<Object>> ENTITY_OR_BLOCK = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.LootContextArg.class, "ENTITY_OR_BLOCK");
    R get(LootContext context);
    ContextKey<?> contextParam();
    static <U> LootContextArg<U> cast(LootContextArg<? extends U> original) {
        return (LootContextArg<U>) original;
    }
    static <R> LootContextArg<R> of(ContextKey<? extends R> contextParam) {
        return new LootContextArg<R>() {
            public R get(LootContext context) {
                return context.getOptional(contextParam);
            }
            public ContextKey<?> contextParam() {
                return contextParam;
            }
        };
    }
    static <R> Codec<LootContextArg<R>> createArgCodec(UnaryOperator<ArgCodecBuilder<R>> consumer) {
        ArgCodecBuilder<R> builder = new ArgCodecBuilder<>();
        return consumer.apply(builder).build();
    }
    public interface Getter<T, R> extends LootContextArg<R> {
        R get(T value);
        ContextKey<? extends T> contextParam();
        default R get(LootContext context) {
            T value = (T) context.getOptional(contextParam());
            return value != null ? get(value) : null;
        }
    }
    public interface SimpleGetter<T> extends LootContextArg<T> {
        ContextKey<? extends T> contextParam();
        default T get(LootContext context) {
            return (T) context.getOptional(contextParam());
        }
    }
    public static final class ArgCodecBuilder<R> {
        private final java.util.Map<String, LootContextArg<R>> known;
        private Codec<LootContextArg<R>> build() {
            java.util.Map<LootContextArg<R>, String> names = new java.util.IdentityHashMap<>();
            known.forEach((key, value) -> names.put(value, key));
            return Codec.STRING.comapFlatMap(key -> known.containsKey(key) ? com.mojang.serialization.DataResult.success(known.get(key)) : com.mojang.serialization.DataResult.error(() -> "Unknown target " + key), arg -> names.getOrDefault(arg, ""));
        }
        private ArgCodecBuilder() {
            this.known = new java.util.LinkedHashMap<>();
        }
        public <T> ArgCodecBuilder<R> anyOf(T[] targets, Function<T, String> nameGetter, Function<T, ? extends LootContextArg<R>> argFactory) {
            for (T each : targets) {
                known.put(nameGetter.apply(each), argFactory.apply(each));
            }
            return this;
        }
        public <T extends StringRepresentable> ArgCodecBuilder<R> anyOf(T[] targets, Function<T, ? extends LootContextArg<R>> argFactory) {
            return anyOf(targets, StringRepresentable::getSerializedName, argFactory);
        }
        public <T extends StringRepresentable & LootContextArg<? extends R>> ArgCodecBuilder<R> anyOf(T[] targets) {
            return anyOf(targets, each -> LootContextArg.cast((LootContextArg) each));
        }
        public ArgCodecBuilder<R> anyEntity(Function<? super ContextKey<? extends Entity>, ? extends LootContextArg<R>> function) {
            return anyOf(LootContext.EntityTarget.values(), each -> function.apply(each.contextParam()));
        }
        public ArgCodecBuilder<R> anyBlockEntity(Function<? super ContextKey<? extends BlockEntity>, ? extends LootContextArg<R>> function) {
            return anyOf(LootContext.BlockEntityTarget.values(), each -> function.apply(each.contextParam()));
        }
        public ArgCodecBuilder<R> anyItemStack(Function<? super ContextKey<? extends ItemInstance>, ? extends LootContextArg<R>> function) {
            return anyOf(LootContext.ItemStackTarget.values(), each -> function.apply(each.contextParam()));
        }
        public ArgCodecBuilder<R> or(String name, LootContextArg<R> arg) {
            known.put(name, arg);
            return this;
        }
    }
}
