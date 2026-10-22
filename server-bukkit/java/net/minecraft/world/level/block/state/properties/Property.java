package net.minecraft.world.level.block.state.properties;

import java.util.List;
import org.basaltmc.bukkit.Unsupported;
public abstract class Property<T> {
    protected Property() {
    }
    public String getName() {
        throw Unsupported.hit("net.minecraft.world.level.block.state.properties.Property.getName");
    }
    public List getPossibleValues() {
        throw Unsupported.hit("net.minecraft.world.level.block.state.properties.Property.getPossibleValues");
    }
    public Class getValueClass() {
        throw Unsupported.hit("net.minecraft.world.level.block.state.properties.Property.getValueClass");
    }
}
