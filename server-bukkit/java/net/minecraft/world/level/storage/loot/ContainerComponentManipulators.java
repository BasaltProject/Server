// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot;
import com.mojang.serialization.Codec;
import java.util.Map;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.core.component.DataComponents;
import net.minecraft.world.item.component.BundleContents;
import net.minecraft.world.item.component.ChargedProjectiles;
import net.minecraft.world.item.component.ItemContainerContents;
public interface ContainerComponentManipulators {
    ContainerComponentManipulator<ItemContainerContents> CONTAINER = new ContainerComponentManipulator<>(DataComponents.CONTAINER, ItemContainerContents.EMPTY);
    ContainerComponentManipulator<BundleContents> BUNDLE_CONTENTS = new ContainerComponentManipulator<>(DataComponents.BUNDLE_CONTENTS, BundleContents.EMPTY);
    ContainerComponentManipulator<ChargedProjectiles> CHARGED_PROJECTILES = new ContainerComponentManipulator<>(DataComponents.CHARGED_PROJECTILES, ChargedProjectiles.EMPTY);
    Map<DataComponentType<?>, ContainerComponentManipulator<?>> ALL_MANIPULATORS = Map.of(CONTAINER.type(), CONTAINER,
        BUNDLE_CONTENTS.type(), BUNDLE_CONTENTS, CHARGED_PROJECTILES.type(), CHARGED_PROJECTILES);
    Codec<ContainerComponentManipulator<?>> CODEC = net.minecraft.core.registries.BuiltInRegistries.DATA_COMPONENT_TYPE.byNameCodec()
        .<ContainerComponentManipulator<?>>comapFlatMap(type -> {
            ContainerComponentManipulator<?> found = ALL_MANIPULATORS.get(type);
            return found != null ? com.mojang.serialization.DataResult.success(found) : com.mojang.serialization.DataResult.error(() -> "No items in component");
        }, ContainerComponentManipulator::type);
}
