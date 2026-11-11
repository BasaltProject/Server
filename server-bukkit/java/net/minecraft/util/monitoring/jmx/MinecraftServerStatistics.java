// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.monitoring.jmx;

import javax.management.Attribute;
import javax.management.AttributeList;
import javax.management.DynamicMBean;
import javax.management.MBeanInfo;
import net.minecraft.server.MinecraftServer;

public final class MinecraftServerStatistics implements DynamicMBean {
    private static final MBeanInfo kInfo = new MBeanInfo(MinecraftServerStatistics.class.getName(), "Minecraft server statistics", null, null, null, null);

    private MinecraftServerStatistics() {
    }

    public static void registerJmxMonitoring(MinecraftServer server) {
    }

    @Override
    public Object getAttribute(String attribute) {
        return null;
    }

    @Override
    public void setAttribute(Attribute attribute) {
    }

    @Override
    public AttributeList getAttributes(String[] attributes) {
        return new AttributeList();
    }

    @Override
    public AttributeList setAttributes(AttributeList attributes) {
        return new AttributeList();
    }

    @Override
    public Object invoke(String actionName, Object[] params, String[] signature) {
        return null;
    }

    @Override
    public MBeanInfo getMBeanInfo() {
        return kInfo;
    }
}
