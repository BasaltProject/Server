package net.minecraft.world.level.block.state.properties;

import java.util.List;
import java.util.Optional;
import org.basaltmc.bukkit.Unsupported;
public class EnumProperty<T> extends Property {
    protected EnumProperty() {
    }
    public String getName() {
        throw Unsupported.hit("net.minecraft.world.level.block.state.properties.EnumProperty.getName");
    }
    public List getPossibleValues() {
        throw Unsupported.hit("net.minecraft.world.level.block.state.properties.EnumProperty.getPossibleValues");
    }
    public Optional getValue(String name) {
        throw Unsupported.hit("net.minecraft.world.level.block.state.properties.EnumProperty.getValue");
    }
    public Class getValueClass() {
        throw Unsupported.hit("net.minecraft.world.level.block.state.properties.EnumProperty.getValueClass");
    }
}
