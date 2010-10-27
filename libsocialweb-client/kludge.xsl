<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet xmlns:gi="http://www.gtk.org/introspection/core/1.0"
                xmlns:glib="http://www.gtk.org/introspection/glib/1.0"
                xmlns:c="http://www.gtk.org/introspection/c/1.0"
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
		version="1.0">

  <xsl:template match="@*|node()">
    <xsl:copy>
      <xsl:apply-templates select="@*|node()"/>
    </xsl:copy>
  </xsl:template>

  <xsl:template match="glib:signal[@name='items-added' or
    @name='items-changed' or @name='items-removed']">
    <glib:signal name="{@name}">
      <return-value transfer-ownership="none">
        <type name="none"/>
      </return-value>
      <parameters>
        <parameter name="items" transfer-ownership="none">
          <type name="GLib.List" c:type="GList*">
            <type name="SocialWebClient.Item" c:type="SwItem*"/>
          </type>
        </parameter>
      </parameters>
    </glib:signal>
  </xsl:template>

</xsl:stylesheet>
