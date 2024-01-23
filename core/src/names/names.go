package names

import (
	"crypto/rand"
	"math/big"
)

var (
	left = [...]string{
		"admiring",
		"adoring",
		"affectionate",
		"agitated",
		"amazing",
		"angry",
		"awesome",
		"beautiful",
		"blissful",
		"bold",
		"boring",
		"brave",
		"busy",
		"charming",
		"clever",
		"compassionate",
		"competent",
		"condescending",
		"confident",
		"cool",
		"cranky",
		"crazy",
		"dazzling",
		"determined",
		"distracted",
		"dreamy",
		"eager",
		"ecstatic",
		"elastic",
		"elated",
		"elegant",
		"eloquent",
		"epic",
		"exciting",
		"fervent",
		"festive",
		"flamboyant",
		"focused",
		"friendly",
		"frosty",
		"funny",
		"gallant",
		"gifted",
		"goofy",
		"gracious",
		"great",
		"happy",
		"hardcore",
		"heuristic",
		"hopeful",
		"hungry",
		"infallible",
		"inspiring",
		"intelligent",
		"interesting",
		"jolly",
		"jovial",
		"keen",
		"kind",
		"laughing",
		"loving",
		"lucid",
		"magical",
		"modest",
		"musing",
		"mystifying",
		"naughty",
		"nervous",
		"nice",
		"nifty",
		"nostalgic",
		"objective",
		"optimistic",
		"peaceful",
		"pedantic",
		"pensive",
		"practical",
		"priceless",
		"quirky",
		"quizzical",
		"recursing",
		"relaxed",
		"reverent",
		"romantic",
		"sad",
		"serene",
		"sharp",
		"silly",
		"sleepy",
		"stoic",
		"strange",
		"stupefied",
		"suspicious",
		"sweet",
		"tender",
		"thirsty",
		"trusting",
		"unruffled",
		"upbeat",
		"vibrant",
		"vigilant",
		"vigorous",
		"wizardly",
		"wonderful",
		"xenodochial",
		"youthful",
		"zealous",
		"zen",
	}

	right = [...]string{
		"fawn",
		"peacock",
		"civet",
		"seastar",
		"pigeon",
		"bull",
		"bumblebee",
		"crocodile",
		"elephant",
		"baboon",
		"porcupine",
		"wolverine",
		"sparrow",
		"manatee",
		"possum",
		"swallow",
		"wildcat",
		"bandicoot",
		"labradoodle",
		"dragonfly",
		"tarsier",
		"chameleon",
		"boykin",
		"puffin",
		"bison",
		"llama",
		"kitten",
		"stinkbug",
		"macaw",
		"parrot",
		"prawn",
		"panther",
		"dogfish",
		"fennec",
		"frigatebird",
		"turkey",
		"cockatoo",
		"neanderthal",
		"crow",
		"gopher",
		"reindeer",
		"earwig",
		"anaconda",
		"panda",
		"ant",
		"puppy",
		"moose",
		"binturong",
		"wildebeest",
		"lovebird",
		"ferret",
		"persian",
		"dalmatian",
		"bird",
		"umbrellabird",
		"kingfisher",
		"kangaroo",
		"stallion",
		"ostrich",
		"owl",
		"affenpinscher",
		"caiman",
		"octopus",
		"meerkat",
		"buck",
		"donkey",
		"quetzal",
		"chamois",
		"sponge",
		"hamster",
		"orangutan",
		"uakari",
		"doberman",
		"dormouse",
		"ocelot",
		"sparrow",
		"spitz",
		"stoat",
		"dragonfly",
		"cougar",
		"alligator",
		"walrus",
		"frog",
		"tiger",
		"armadillo",
		"chinchilla",
		"crab",
		"squid",
		"calf",
		"shrew",
		"dolphin",
		"dingo",
		"turtle",
		"chimpanzee",
		"armadillo",
		"rabbit",
		"basking",
		"coyote",
		"chinook",
		"osprey",
		"fly",
		"tiffany",
		"dodo",
		"worm",
		"cat",
		"warthog",
		"peccary",
		"shark",
		"pony",
		"monkey",
		"swan",
		"whippet",
		"beagle",
		"cougar",
		"anteater",
		"quail",
		"liger",
		"cheetah",
		"woodpecker",
		"egret",
		"eagle",
		"moose",
		"warthog",
		"snail",
		"budgie",
		"molly",
		"magpie",
		"rhinoceros",
		"elephant",
		"kudu",
		"wombat",
		"goat",
		"lamb",
		"tropicbird",
		"human",
		"hog",
		"tang",
		"lemur",
		"ox",
		"dog",
		"lizard",
		"echidna",
		"wallaby",
		"hawk",
		"dove",
		"jellyfish",
		"sloth",
		"macaque",
		"starfish",
		"guppy",
		"deer",
		"impala",
		"porpoise",
		"gazelle",
		"bichon",
		"seal",
		"wolf",
		"mole",
		"narwhal",
		"hedgehog",
		"sheep",
		"horse",
		"bluetick",
		"colt",
		"wildebeest",
		"piranha",
		"basenji",
		"mallard",
		"bear",
		"bird",
		"badger",
		"hammerhead",
		"kangaroo",
		"mule",
		"weasel",
		"dogfish",
		"dachsbracke",
		"oyster",
		"bat",
		"python",
		"coati",
		"platypus",
		"salamander",
		"cat",
		"caterpillar",
		"giraffe",
		"snake",
		"kid",
		"falcon",
		"robin",
		"tern",
		"dingo",
		"bolognese",
		"drake",
		"goose",
		"rat",
		"iguana",
		"quail",
		"mouse",
		"roebuck",
		"fish",
		"poodle",
		"frog",
		"wolverine",
		"chinchilla",
		"bobcat",
		"carolina",
		"shepherd",
		"snail",
		"mandrill",
		"leopard",
		"echidna",
		"rabbit",
		"bison",
		"barracuda",
		"foal",
		"ass",
		"eagle",
		"octopus",
		"avocet",
		"siamese",
		"dodo",
		"yorkie",
		"cockroach",
		"wallaroo",
		"tiger",
		"woodlouse",
		"fossa",
		"buffalo",
		"zorse",
		"albatross",
		"indri",
		"seahorse",
		"lemur",
		"louse",
		"ostrich",
		"millipede",
		"joey",
		"pinscher",
		"dachshund",
		"pelican",
		"chihuahua",
		"dogo",
		"wasp",
		"siberian",
		"yak",
		"stingray",
		"foxhound",
		"sheep",
		"stork",
		"horse",
		"monkey",
		"waterbuck",
		"dunker",
		"cuscus",
		"ibis",
		"giraffe",
		"aardvark",
		"hummingbird",
		"otter",
		"pike",
		"pika",
		"stickbug",
		"pelican",
		"dugong",
		"bongo",
		"lemming",
		"shrimp",
		"piglet",
		"gemsbok",
		"tuatara",
		"rottweiler",
		"ewe",
		"coati",
		"cichlid",
		"akita",
		"gharial",
		"duck",
		"steer",
		"setter",
		"pufferfish",
		"donkey",
		"mink",
		"macaw",
		"wolfhound",
		"ram",
		"ant",
		"rat",
		"marten",
		"crab",
		"koala",
		"starfish",
		"partridge",
		"chipmunk",
		"ibex",
		"maltese",
		"clumber",
		"butterfly",
		"flamingo",
		"opossum",
		"parrot",
		"mastiff",
		"okapi",
		"salmon",
		"tapir",
		"adelie",
		"lynx",
		"basilisk",
		"oyster",
		"chipmunk",
		"locust",
		"dog",
		"cottontop",
		"hyena",
		"oriole",
		"cobra",
		"pug",
		"monitor",
		"mandrill",
		"antelope",
		"chinstrap",
		"zebra",
		"chicken",
		"mule",
		"seal",
		"goat",
		"gull",
		"caterpillar",
		"tamarin",
		"wrasse",
		"woodchuck",
		"otter",
		"penguin",
		"porcupine",
		"bear",
		"ferret",
		"dusky",
		"nightingale",
		"bat",
		"jaguar",
		"humboldt",
		"ermine",
		"saola",
		"emu",
		"lobster",
		"weasel",
		"nightingale",
		"hound",
		"bombay",
		"platypus",
		"uguisu",
		"scorpion",
		"fox",
		"jerboa",
		"zebu",
		"lion",
		"zonkey",
		"ragdoll",
		"caracal",
		"bee",
		"kiwi",
		"puma",
		"jackal",
		"malamute",
		"mayfly",
		"baboon",
		"terrier",
		"jellyfish",
		"vicuna",
		"penguin",
		"muskrat",
		"zebra",
		"burmese",
		"orangutan",
		"himalayan",
		"newt",
		"cow",
		"fish",
		"puffin",
		"chin",
		"anteater",
		"beaver",
		"canary",
		"hamster",
		"sloth",
		"collie",
		"heron",
		"gopher",
		"magpie",
		"flounder",
		"opossum",
		"pademelon",
		"capybara",
		"boar",
		"turkey",
		"musk-ox",
		"bulldog",
		"pronghorn",
		"reindeer",
		"llama",
		"pygmy",
		"kinkajou",
		"cuttlefish",
		"cub",
		"bloodhound",
		"squirrel",
		"gander",
		"moorhen",
		"emu",
		"javanese",
		"birman",
		"harrier",
		"tortoise",
		"antelope",
		"gnu",
		"kingfisher",
		"wasp",
		"olm",
		"havanese",
		"canaan",
		"lizard",
		"ocelot",
		"mist",
		"hare",
		"discus",
		"cony",
		"orca",
		"rooster",
		"peacock",
		"akbash",
		"somali",
		"beaver",
		"mouse",
		"eland",
		"squirrel",
		"serval",
		"chimpanzee",
		"snowshoe",
		"toucan",
		"catfish",
		"lynx",
		"coyote",
		"bunny",
		"retriever",
		"cow",
		"balinese",
		"vulture",
		"coral",
		"leopard",
		"raccoon",
		"okapi",
		"kakapo",
		"whale",
		"bonobo",
		"moray",
		"cormorant",
		"bracke",
		"camel",
		"markhor",
		"rockhopper",
		"neapolitan",
		"woodpecker",
		"hippopotamus",
		"puma",
		"camel",
		"alligator",
		"heron",
		"axolotl",
		"argentino",
		"human",
		"mongoose",
		"drever",
		"quokka",
		"elk",
		"wombat",
		"civet",
		"panther",
		"gar",
		"lionfish",
		"snake",
		"crane",
		"newt",
		"raven",
		"tortoise",
		"chicadee",
		"pig",
		"manatee",
		"centipede",
		"numbat",
		"falcon",
		"angelfish",
		"chamois",
		"rhinoceros",
		"shark",
		"flamingo",
		"pheasant",
		"ladybird",
		"grasshopper",
		"greyhound",
		"lemming",
		"pig",
		"marmoset",
		"eel",
		"yorkiepoo",
		"mosquito",
		"quoll",
		"chick",
		"guanaco",
		"walrus",
		"badger",
		"ainu",
		"squid",
		"pekingese",
		"gerbil",
		"duck",
		"rattlesnake",
		"tapir",
		"lobster",
		"catfish",
		"mustang",
		"wallaby",
		"mongrel",
		"butterfly",
		"booby",
		"fox",
		"rattlesnake",
		"cockroach",
		"tadpole",
		"lark",
		"ape",
		"mare",
		"tetra",
		"dhole",
		"cesky",
		"raccoon",
		"newfoundland",
		"marmoset",
		"stag",
		"bullfrog",
		"crocodile",
		"lion",
		"barb",
		"wolf",
		"beetle",
		"pointer",
		"meerkat",
		"owl",
		"reptile",
		"fousek",
		"gibbon",
		"budgerigar",
		"swan",
		"hartebeest",
		"cassowary",
		"oryx",
		"alpaca",
		"gerbil",
		"chameleon",
		"vulture",
		"barracuda",
		"insect",
		"hen",
		"hare",
		"polecat",
		"fly",
		"yak",
		"seahorse",
		"spider",
		"eel",
		"burro",
		"crane",
		"bandicoot",
		"hedgehog",
		"dromedary",
		"goose",
		"budgerigar",
		"dolphin",
		"dormouse",
		"duckbill",
		"springbok",
		"mongoose",
		"bobcat",
		"gecko",
		"hornet",
		"iguana",
		"koala",
		"marmot",
		"skink",
		"deer",
		"filly",
		"barnacle",
		"appenzeller",
		"doe",
		"gecko",
		"mole",
		"mau",
		"termite",
		"salamander",
		"parakeet",
		"finch",
		"hippopotamus",
		"hummingbird",
		"cheetah",
		"albatross",
		"jaguar",
		"toad",
		"hyena",
		"gorilla",
		"skunk",
		"impala",
		"jackal",
		"skunk",
		"grouse",
		"moth",
		"caribou",
		"dugong",
		"gorilla",
		"chicken",
		"buffalo",
	}
)

func Random() string {
	var l, r string
	for i := 0; i < 100; i++ {
		lIndex, err := rand.Int(rand.Reader, big.NewInt(int64(len(left))))
		if err != nil {
			return ""
		}
		rIndex, err := rand.Int(rand.Reader, big.NewInt(int64(len(right))))
		if err != nil {
			return ""
		}
		l = left[lIndex.Int64()]
		r = right[rIndex.Int64()]
		if l[0] == r[0] {
			break
		}
	}
	return l + "-" + r
}