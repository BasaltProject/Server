// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.vehicle.minecart;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.phys.Vec3;
public class Minecart extends AbstractMinecart {
    public Minecart(EntityType<?> type, Level level) {
        super(type, level);
    }
    public InteractionResult interact(Player player, InteractionHand hand, Vec3 location) {
        return net.minecraft.world.InteractionResult.PASS;
    }
    protected Item getDropItem() {
        return net.minecraft.world.item.Items.MINECART;
    }
    public ItemStack getPickResult() {
        return new ItemStack(net.minecraft.world.item.Items.MINECART);
    }
    public boolean isRideable() {
        return false;
    }
    public void tick() {
    }
    protected void positionRider(Entity passenger, Entity.MoveFunction moveFunction) {
    }
}
