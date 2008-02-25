<map version="0.8.0">
<!-- To view this file, download free mind mapping software FreeMind from http://freemind.sourceforge.net -->
<node CREATED="1202314683743" ID="Freemind_Link_15388610" MODIFIED="1202314854242" TEXT="Userp">
<node CREATED="1202314855424" ID="_" MODIFIED="1202314864077" POSITION="right" TEXT="Structure">
<node CREATED="1202314865129" ID="Freemind_Link_211147694" MODIFIED="1202314878969" TEXT="TypeSpace">
<node CREATED="1202314879933" ID="Freemind_Link_329789925" MODIFIED="1202314999843" TEXT="Manages type objects and handles"/>
</node>
<node CREATED="1202315003090" ID="Freemind_Link_1700390365" MODIFIED="1202315033274" TEXT="UserpReader">
<node CREATED="1202315034996" ID="Freemind_Link_187860818" MODIFIED="1202315057038" TEXT="Allows seek and read operations on a data source"/>
<node CREATED="1202315063998" ID="Freemind_Link_309271204" MODIFIED="1202315080763" TEXT="Exists in context of TypeSpace"/>
<node CREATED="1202315085377" ID="Freemind_Link_328254248" MODIFIED="1202315234074" TEXT="Presents data in the form of a type and a buffer of const data.&#xa;The Codec converts from the const data to a user-understood datatype"/>
<node CREATED="1202315239929" ID="Freemind_Link_1055922523" MODIFIED="1202315278022" TEXT="Has lots of helper functions that wrap methods in the codec"/>
</node>
<node CREATED="1202315283005" ID="Freemind_Link_521003634" MODIFIED="1202315285369" TEXT="Codec">
<node CREATED="1202315286415" ID="Freemind_Link_666706960" MODIFIED="1202315300881" TEXT="Knows how to read/write values of a type"/>
<node CREATED="1202315301390" ID="Freemind_Link_1900981926" MODIFIED="1202315309431" TEXT="Never seen by the user"/>
<node CREATED="1202315309914" ID="Freemind_Link_1883867592" MODIFIED="1202315331379" TEXT="A function of a type and additional encoder properties"/>
</node>
</node>
<node CREATED="1202315333295" ID="Freemind_Link_420326359" MODIFIED="1202315337046" POSITION="right" TEXT="API">
<node CREATED="1202315337964" FOLDED="true" ID="Freemind_Link_244148814" MODIFIED="1202315939850" TEXT="TTypeSpace">
<node CREATED="1202315347301" ID="Freemind_Link_466942403" MODIFIED="1202315352185" TEXT="CreateType"/>
<node CREATED="1202315480167" ID="Freemind_Link_2220339" MODIFIED="1202315489524" TEXT="InitAsInteger"/>
<node CREATED="1202315352924" ID="Freemind_Link_787671882" MODIFIED="1202315357248" TEXT="InitAsEnum"/>
<node CREATED="1202315357731" ID="Freemind_Link_1691076372" MODIFIED="1202315364426" TEXT="InitAsUnion"/>
<node CREATED="1202315364709" ID="Freemind_Link_956435554" MODIFIED="1202315474273" TEXT="InitAsArray"/>
<node CREATED="1202315474904" ID="Freemind_Link_1722909852" MODIFIED="1202315478015" TEXT="InitAsRecord"/>
<node CREATED="1202315593342" ID="Freemind_Link_1594620084" MODIFIED="1202315596831" TEXT="InitAsUnionAll"/>
</node>
<node CREATED="1202315603396" ID="Freemind_Link_1246430739" MODIFIED="1202315946121" TEXT="TUserpReader"/>
<node CREATED="1202315629389" ID="Freemind_Link_1504128951" MODIFIED="1202315949643" TEXT="TUserpWriter"/>
<node CREATED="1202315639095" ID="Freemind_Link_563673859" MODIFIED="1202315952546" TEXT="TOctetSequence"/>
<node CREATED="1202315846132" ID="Freemind_Link_1503709651" MODIFIED="1202315932170" TEXT="TType"/>
</node>
<node CREATED="1202315966222" ID="Freemind_Link_274847093" MODIFIED="1202315976337" POSITION="right" TEXT="Timeline">
<node CREATED="1202315977526" ID="Freemind_Link_1799577947" MODIFIED="1202316048401" TEXT="Feb 3-9">
<node CREATED="1202316134817" ID="Freemind_Link_1469415150" MODIFIED="1202316436955" TEXT="OctetSequence - make it usable for bytes"/>
<node CREATED="1202316157722" ID="Freemind_Link_980866618" MODIFIED="1202316648707" TEXT="Write codec for Integer(N)"/>
<node CREATED="1202316248457" ID="Freemind_Link_48052132" MODIFIED="1202316420388" TEXT="OctetSequenceWriter - bare essentials, for bytes"/>
<node CREATED="1202316438784" ID="Freemind_Link_360628976" MODIFIED="1202316639743" TEXT="Get Integer(N) working in TypeSpace"/>
<node CREATED="1202316608766" ID="Freemind_Link_1609776426" MODIFIED="1202316632524" TEXT="Add Integer(N) functions for Reader and Writer"/>
<node CREATED="1202316460640" ID="Freemind_Link_9269604" MODIFIED="1202316468763" TEXT="Write a file of an integer, and then read it"/>
</node>
<node CREATED="1202316048930" ID="Freemind_Link_1433995383" MODIFIED="1202316058010" TEXT="Feb 10-16">
<node CREATED="1202316483658" ID="Freemind_Link_698759951" MODIFIED="1202316521859" TEXT="Write codec for Array(Normal)"/>
<node CREATED="1202316669953" ID="Freemind_Link_1737300783" MODIFIED="1202316679454" TEXT="Add arrays to TypeSpace"/>
<node CREATED="1202316679681" ID="Freemind_Link_806982656" MODIFIED="1202316696615" TEXT="Add arrays to UserpReader and UserpWriter"/>
<node CREATED="1202316524528" ID="Freemind_Link_157088519" MODIFIED="1202316547677" TEXT="Write a file of an array of integers, and read it"/>
<node CREATED="1202316550817" ID="Freemind_Link_1911463521" MODIFIED="1202316560819" TEXT="Write codec for Record(Normal)"/>
<node CREATED="1202316699345" ID="Freemind_Link_1564044474" MODIFIED="1202316705093" TEXT="Add records to TypeSpace"/>
<node CREATED="1202316705391" ID="Freemind_Link_1340477117" MODIFIED="1202316716832" TEXT="Add records to UserpReader and UserpWriter"/>
<node CREATED="1202316561302" ID="Freemind_Link_328055609" MODIFIED="1202316602288" TEXT="Write a file of a record of arrays or integers or records, and read it"/>
</node>
<node CREATED="1202316058299" ID="Freemind_Link_1449609147" MODIFIED="1202316065316" TEXT="Feb 17-23">
<node CREATED="1202316721999" FOLDED="true" ID="Freemind_Link_441650272" MODIFIED="1202316737357" TEXT="Implement Union">
<node CREATED="1202316737998" ID="Freemind_Link_881059062" MODIFIED="1202316746654" TEXT="Add its type to TypeSpace"/>
<node CREATED="1202316746954" ID="Freemind_Link_1147575944" MODIFIED="1202316761639" TEXT="Add subtype-selection to UserpReader"/>
<node CREATED="1202316762741" ID="Freemind_Link_1890629040" MODIFIED="1202316772779" TEXT="Add subtype-selection to UserpWriter"/>
<node CREATED="1202316777215" ID="Freemind_Link_958521804" MODIFIED="1202316792305" TEXT="Create its codec, for non-packed mode"/>
<node CREATED="1202316793044" ID="Freemind_Link_1895486714" MODIFIED="1202316802135" TEXT="Read and write it"/>
</node>
<node CREATED="1202316815878" ID="Freemind_Link_760015740" MODIFIED="1202316819163" TEXT="Implement Enum"/>
<node CREATED="1202317000416" ID="Freemind_Link_1791524322" MODIFIED="1202317005944" TEXT="Implement UnionAll"/>
</node>
<node CREATED="1202316065738" ID="Freemind_Link_1746206902" MODIFIED="1202316079754" TEXT="Feb 24-">
<node CREATED="1202317049882" ID="Freemind_Link_802589217" MODIFIED="1202317123910" TEXT="Implement the Reader and Writer subclasses which operate on separated data and typerefs"/>
<node CREATED="1202317126405" ID="Freemind_Link_1052178559" MODIFIED="1202317185146" TEXT="Add the attribute and tagging methods to the types"/>
</node>
<node CREATED="1202316080216" ID="Freemind_Link_1705142523" MODIFIED="1202316097954" TEXT="Mar 2-8">
<node CREATED="1202316839336" ID="Freemind_Link_97024361" MODIFIED="1202316855681" TEXT="Implement BigInteger read/write/codec"/>
<node CREATED="1202316890048" ID="Freemind_Link_1895413581" MODIFIED="1202316899261" TEXT="Implement streamed arrays and records"/>
<node CREATED="1202316980286" ID="Freemind_Link_695507150" MODIFIED="1202316987800" TEXT="Implement indexed arrays and records"/>
</node>
<node CREATED="1202316098421" ID="Freemind_Link_137345527" MODIFIED="1202316104763" TEXT="Mar 9-15">
<node CREATED="1202316914243" ID="Freemind_Link_561861342" MODIFIED="1202316956627" TEXT="Implement the bitpacking methods for OctetSequence iterators, and for OctetSequenceWriter"/>
<node CREATED="1202316860577" ID="Freemind_Link_1549008577" MODIFIED="1202316881822" TEXT="Implement bit-packed Arrays and records"/>
<node CREATED="1202316902693" ID="Freemind_Link_721543337" MODIFIED="1202316912260" TEXT="Implement bit-packed Unions"/>
</node>
<node CREATED="1202316105143" ID="Freemind_Link_1979648255" MODIFIED="1202316113968" TEXT="Mar 16-22">
<node CREATED="1202316962997" ID="Freemind_Link_783107329" MODIFIED="1202316965857" TEXT="Documentation"/>
</node>
<node CREATED="1202316114435" ID="Freemind_Link_545620302" MODIFIED="1202316131308" TEXT="Mar 23-29">
<node CREATED="1202317302391" ID="Freemind_Link_460411357" MODIFIED="1202317305200" TEXT="CmdlineGL"/>
</node>
</node>
</node>
</map>
