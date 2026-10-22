package net.minecraft.world.level.block.state.properties;

import java.util.List;
import org.basaltmc.bukkit.Unsupported;
public class IntegerProperty extends Property {
    protected IntegerProperty() {
    }
    public String getName() {
        throw Unsupported.hit("net.minecraft.world.level.block.state.properties.IntegerProperty.getName");
    }
    public List getPossibleValues() {
        throw Unsupported.hit("net.minecraft.world.level.block.state.properties.IntegerProperty.getPossibleValues");
    }
}
