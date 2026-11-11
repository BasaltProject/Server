// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.variant;
import net.minecraft.resources.Identifier;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.core.Registry;
import net.minecraft.core.RegistryAccess;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class VariantUtils {
    public static final String TAG_VARIANT = "variant";
    public static <T> Holder<T> getDefaultOrAny(RegistryAccess registryAccess, ResourceKey<T> id) {
        Registry<T> registry = registryAccess.lookupOrThrow(id.registryKey());
        return registry.get(id).or(registry::getAny).orElseThrow();
    }
    public static <T> Holder<T> getAny(RegistryAccess registryAccess, ResourceKey<? extends Registry<T>> registryId) {
        return registryAccess.lookupOrThrow(registryId).getAny().orElseThrow();
    }
    public static <T> void writeVariant(ValueOutput output, Holder<T> holder) {
        holder.unwrapKey().ifPresent(key -> output.store("variant", Identifier.CODEC, key.identifier()));
    }
    public static <T> Optional<Holder<T>> readVariant(ValueInput input, ResourceKey<? extends Registry<T>> registryId) {
        return input.read("variant", Identifier.CODEC).map(id -> ResourceKey.create(registryId, id)).flatMap(input.lookup()::get);
    }
    public static <T extends PriorityProvider<SpawnContext, ?>> Optional<Holder.Reference<T>> selectVariantToSpawn(SpawnContext context, ResourceKey<Registry<T>> variantRegistry) {
        return Optional.empty();


    }
}
