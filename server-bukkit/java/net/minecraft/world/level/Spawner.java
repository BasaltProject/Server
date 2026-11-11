// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;
import net.minecraft.ChatFormatting;
import net.minecraft.network.chat.CommonComponents;
import java.util.function.Consumer;
import net.minecraft.network.chat.Component;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.item.component.TypedEntityData;
import net.minecraft.world.level.block.entity.BlockEntityType;
public interface Spawner {
    void setEntityId(EntityType<?> type, RandomSource random);
    void setEntityData(TypedEntityData<EntityType<?>> entityData, RandomSource random);
    static void appendHoverText(TypedEntityData<BlockEntityType<?>> data, Consumer<Component> consumer, String nextSpawnDataTagKey) {
        Component name = getSpawnEntityDisplayName(data, nextSpawnDataTagKey);
        if (name != null) {
            consumer.accept(name);
            return;
        }
        consumer.accept(CommonComponents.EMPTY);
        consumer.accept(Component.translatable("block.minecraft.spawner.desc1").withStyle(ChatFormatting.GRAY));
        consumer.accept(CommonComponents.space().append(Component.translatable("block.minecraft.spawner.desc2").withStyle(ChatFormatting.BLUE)));
    }
    static Component getSpawnEntityDisplayName(TypedEntityData<BlockEntityType<?>> data, String nextSpawnDataTagKey) {
        if (data == null) {
            return null;
        }
        return data.getUnsafe().getCompound(nextSpawnDataTagKey).flatMap(next -> next.getCompound("entity"))
            .flatMap(entity -> entity.read("id", EntityType.CODEC))
            .map(type -> (Component) Component.translatable(type.getDescriptionId()).withStyle(ChatFormatting.GRAY)).orElse(null);
    }
}
