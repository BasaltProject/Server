// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.dispenser;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.block.DispenserBlock;
import net.minecraft.world.phys.AABB;
public class EquipmentDispenseItemBehavior extends DefaultDispenseItemBehavior {
    public static final EquipmentDispenseItemBehavior INSTANCE = new EquipmentDispenseItemBehavior();
    protected ItemStack execute(BlockSource source, ItemStack dispensed) {
        return dispenseEquipment(source, dispensed) ? dispensed : super.execute(source, dispensed);
    }
    public static boolean dispenseEquipment(BlockSource source, ItemStack dispensed) {
        BlockPos front = source.pos().relative(source.state().getValue(DispenserBlock.FACING));
        List<LivingEntity> wearers = source.level().getEntitiesOfClass(LivingEntity.class, new AABB(front), e -> e.canEquipWithDispenser(dispensed));
        if (wearers.isEmpty()) {
            return false;
        }
        LivingEntity wearer = wearers.getFirst();
        EquipmentSlot slot = wearer.getEquipmentSlotForItem(dispensed);
        wearer.setItemSlot(slot, dispensed.split(1));
        if (wearer instanceof Mob mob) {
            mob.setGuaranteedDrop(slot);
            mob.setPersistenceRequired();
        }
        return true;
    }
}
