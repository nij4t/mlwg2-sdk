<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################# 
|	$Id: //WIFI_SOC/release/SDK_4_1_0_0/source/user/samba-3.0.37/docs-xml/xslt/db2latex-xsl/xsl/abstract.mod.xsl#1 $
|- #############################################################################
|	$Author: bruce.chang $
+ ############################################################################## -->

<xsl:stylesheet
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>

	<doc:reference id="abstract" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: //WIFI_SOC/release/SDK_4_1_0_0/source/user/samba-3.0.37/docs-xml/xslt/db2latex-xsl/xsl/abstract.mod.xsl#1 $
			</releaseinfo>
			<authorgroup>
				&ramon;
				&james;
			</authorgroup>
			<copyright>
				<year>2000</year><year>2001</year><year>2002</year><year>2003</year>
				<holder>Ramon Casellas</holder>
			</copyright>
			<revhistory>
				<doc:revision rcasver="1.6">&rev_2003_05;</doc:revision>
			</revhistory>
		</referenceinfo>
		<title>Abstracts <filename>abstract.mod.xsl</filename></title>
		<partintro>
			<para>The file <filename>abstract.mod.xsl</filename> contains the
			XSL template for <doc:db>abstract</doc:db>.</para>
		</partintro>
	</doc:reference>

	<doc:template basename="abstract" match="abstract" xmlns="">
		<refpurpose>Process <doc:db>abstract</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Uses the &latex; <function condition='env'>abstract</function> environment
				to format <quote>abstracts</quote> as blocks.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>Currently, the <doc:db>title</doc:db> is not honoured.</para>
			<para>The &db2latex; template for <doc:db>abstract</doc:db> is intended for use with <doc:db>article</doc:db> and <doc:db>book</doc:db>, only.</para>
		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_article;
				&test_book;
				&test_ddh;
				&test_ieeebiblio;
				&test_varioref;
			</simplelist>
		</doc:samples>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara>&mapping;</simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template match="abstract">
		<xsl:variable name="keyword">
			<xsl:value-of select="local-name(.)"/>
			<xsl:if test="title">
				<!-- choose a different mapping -->
				<xsl:text>-title</xsl:text>
			</xsl:if>
		</xsl:variable>
		<xsl:call-template name="map.begin">
			<xsl:with-param name="keyword" select="$keyword"/>
		</xsl:call-template>
		<xsl:call-template name="content-templates"/>
		<xsl:call-template name="map.end">
			<xsl:with-param name="keyword" select="$keyword"/>
		</xsl:call-template>
	</xsl:template>

</xsl:stylesheet>


