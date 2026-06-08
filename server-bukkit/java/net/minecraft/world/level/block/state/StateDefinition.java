package net.minecraft.world.level.block.state;

import java.util.Collection;
import net.minecraft.world.level.block.state.properties.Property;
import org.basaltmc.bukkit.Unsupported;
public class StateDefinition<O, S> {
    protected StateDefinition() {
    }
    public Collection getProperties() {
        throw Unsupported.hit("net.minecraft.world.level.block.state.StateDefinition.getProperties");
    }
    public Property getProperty(String name) {
        throw Unsupported.hit("net.minecraft.world.level.block.state.StateDefinition.getProperty");
    }
}
