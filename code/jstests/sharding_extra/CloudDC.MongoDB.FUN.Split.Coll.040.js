(function() {
    'use strict';

        var st = new ShardingTest({shards: 3, mongos: 1});
        var primarycs=st.configRS.getPrimary();var configSecondaryList = st.configRS.getSecondaries();var mgs=st.s0;
		var cl='asdasdasfagchnfghfgpasdasdasfagchnfghfgpasdasdasfagchnfghfgpasdasdasfagchnfghfgpasdasdasfagchnfghfgp';
		var dbb='asdasdasfagchn'
        var admin=mgs.getDB('admin');
        var cfg=mgs.getDB('config');
        var coll=mgs.getCollection(dbb+"."+cl);
        var coll1=mgs.getCollection("testDB.foo1");
        var testdb=mgs.getDB(dbb);
        st.stopBalancer();
        assert.commandWorked(admin.runCommand({enableSharding:dbb}));
        assert.commandWorked(admin.runCommand({shardCollection:dbb+"."+cl,key:{a:1}}));
        jsTest.log("-------------------insert data-------------------");
		st.disableAutoSplit();
		
		var bigString = "";
		for (var i =1; i < 301; i++) {
	    var floatn=-i;
        assert.writeOK(coll.insert({a: floatn,b:floatn}));
    }
         
		
        for (var i=1;i<10;i++){
			var doc={};
			var key="name"+i
		doc[key]=1
        assert.commandWorked(coll.ensureIndex(doc));}
        printShardingStatus(st.config,false);
		var array=[];
        for (var j=0;j<1;){
        
		var mid1=1234567123456712345671234567123456712345671234567123456712345671234567123456712345671234567123456712345671234567
        var mid2=dbb+dbb+dbb+cl+cl+cl+cl
		
		assert.commandWorked(admin.runCommand({split: dbb+"."+cl,middle :{a : mid1}}));
		assert.commandWorked(admin.runCommand({split: dbb+"."+cl,middle :{a : mid2}}));
		assert.writeOK(coll.insert({a: -10000000, "name1": 20}));
		assert.writeOK(coll.insert({a: 4000000, "name1": 20}));
        printShardingStatus(st.config,false);
        jsTest.log("-------------------confirm chunks normal-------------------");
		

		jsTest.log("-------------------confirm size normal-------------------");
        jsTest.log(j);
        var chunks = cfg.chunks.find().toArray();
        var num1 = cfg.chunks.find().itcount();
        var num2 = j + 3;
        assert.eq(num1,num2);
        jsTest.log("-------------------confirm update normal-------------------");
        jsTest.log(j);
        jsTest.log(num1);
		j++;
        }
        jsTest.log("-------------------create coll1 normal-------------------");
        assert.commandWorked(admin.runCommand({shardCollection:dbb+".foo1",key:{b:1}}));
        assert.writeOK(coll1.insert({b: 10, d: 20}));
		printShardingStatus(st.config,false);
        var shards = cfg.shards.find().toArray();
        //assert.eq("shard0000",shards[0]._id);
        //assert.eq("shard0001",shards[1]._id);
        //assert.eq("shard0002",shards[2]._id);
assert.writeOK(coll.update({a:-2},{"$set":{b:-10000}},false,true));
        assert.eq(302, coll.find().itcount());
		assert.eq(2, coll.find({"name1":20}).itcount());
		assert.eq(1, coll.find({b:-10000}).itcount());
        assert.commandWorked(coll.dropIndex("name1_1"));
        assert.eq(10, coll.getIndexes().length);

        st.stop();
})();

