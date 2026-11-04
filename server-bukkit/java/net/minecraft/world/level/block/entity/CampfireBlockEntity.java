// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;

import net.minecraft.core.BlockPos;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.network.chat.Component;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.block.state.BlockState;
import org.basaltmc.bukkit.facade.TileNbt;

public class CampfireBlockEntity extends BlockEntity implements net.minecraft.world.Clearable {
    public CampfireBlockEntity(BlockPos worldPosition, BlockState blockState) {
        super(BlockEntityType.named("campfire"), worldPosition, blockState);
    }

    public CampfireBlockEntity(ServerLevel level, BlockPos pos, CompoundTag nbt) {
        super(level, pos, nbt);
    }

    public boolean isLit() {
        return getBlockState().getValue(net.minecraft.world.level.block.state.properties.BlockStateProperties.LIT);
    }

    public boolean placeFood(ServerLevel level, net.minecraft.world.entity.LivingEntity entity, net.minecraft.world.item.ItemStack food) {
        org.bukkit.inventory.ItemStack bukkitFood = org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(food);
        if (!(level.getWorld().getBlockAt(getBlockPos().getX(), getBlockPos().getY(), getBlockPos().getZ()).getState()
                instanceof org.bukkit.block.Campfire campfire)) {
            return false;
        }
        int time = -1;
        java.util.Iterator<org.bukkit.inventory.Recipe> recipes = org.bukkit.Bukkit.recipeIterator();
        while (recipes.hasNext()) {
            if (recipes.next() instanceof org.bukkit.inventory.CampfireRecipe recipe && recipe.getInputChoice().test(bukkitFood)) {
                time = recipe.getCookingTime();
                break;
            }
        }
        if (time < 0) {
            return false;
        }
        for (int slot = 0; slot < campfire.getSize(); slot++) {
            org.bukkit.inventory.ItemStack present = campfire.getItem(slot);
            if (present == null || present.getType().isAir()) {
                campfire.setItem(slot, bukkitFood.asOne());
                campfire.setCookTimeTotal(slot, time);
                campfire.setCookTime(slot, 0);
                campfire.update();
                return true;
            }
        }
        return false;
    }
}
