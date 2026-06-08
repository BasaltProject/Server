package net.minecraft.world.level.block.state.properties;

import java.util.List;
import org.basaltmc.bukkit.Unsupported;
public class BooleanProperty extends Property {
    protected BooleanProperty() {
    }
    public String getName() {
        throw Unsupported.hit("net.minecraft.world.level.block.state.properties.BooleanProperty.getName");
    }
    public List getPossibleValues() {
        throw Unsupported.hit("net.minecraft.world.level.block.state.properties.BooleanProperty.getPossibleValues");
    }
}
