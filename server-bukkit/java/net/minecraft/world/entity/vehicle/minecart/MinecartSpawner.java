// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.vehicle.minecart;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.BaseSpawner;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class MinecartSpawner extends AbstractMinecart {
    public MinecartSpawner(EntityType<? extends MinecartSpawner> type, Level level) {
        super(type, level);
    }
    protected Item getDropItem() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.vehicle.minecart.MinecartSpawner#getDropItem");
    }
    public ItemStack getPickResult() {
        return new ItemStack(net.minecraft.world.item.Items.MINECART);
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    public void handleEntityEvent(byte id) {
    }
    public void tick() {
    }
    public BaseSpawner getSpawner() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.vehicle.minecart.MinecartSpawner#getSpawner");
    }
}
