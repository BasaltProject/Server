// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.Set;
import net.minecraft.util.context.ContextKey;
import net.minecraft.world.attribute.EnvironmentAttribute;
import net.minecraft.world.level.storage.loot.LootContextUser;
public interface EnvironmentAttributeProvider extends LootContextUser {
    static <Self extends EnvironmentAttributeProvider> MapCodec<Self> mapCodec(Codec<EnvironmentAttribute<?>> attributeCodec, Factory<Self> factory) {
        return RecordCodecBuilder.<Self>mapCodec(instance -> instance.group(
            attributeCodec.fieldOf("attribute").forGetter(EnvironmentAttributeProvider::attribute)).apply(instance, factory::create));
    }
    EnvironmentAttribute<?> attribute();
    default Set<ContextKey<?>> getReferencedContextParams() {
        return attribute().isPositional() ? Set.of(net.minecraft.world.level.storage.loot.parameters.LootContextParams.ORIGIN) : Set.of();
    }
    public interface Factory<Self extends EnvironmentAttributeProvider> {
        Self create(EnvironmentAttribute<?> attribute);
    }
}
