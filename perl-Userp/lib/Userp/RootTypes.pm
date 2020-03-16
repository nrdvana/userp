package Userp::RootTypes;
use strict;
use warnings;
use Userp::Type::Any;
use Userp::Type::Symbol;
use Userp::Type::Type;
use Userp::Type::Integer;
use Userp::Type::Choice;
use Userp::Type::Array;
use Userp::Type::Record qw/ ALWAYS OFTEN SELDOM /;
use Exporter 'import';
our @EXPORT_OK= qw(
	type_Any type_Symbol type_Type type_Int type_IntU type_Array type_Record type_Matrix
	type_Byte type_ByteArray type_String
);

our @TABLE;
BEGIN {
	my $add= sub {
		my $class= shift;
		my $name= shift;
		my $type= "Userp::Type::$class"->new(
			scope_idx => 0,
			table_idx => scalar @TABLE,
			name => "Userp.$name",
			@_
		);
		push @TABLE, $type;
		no strict 'refs';
		*{"type_$name"}= sub { $type };
	};
	$add->(Any     => 'Any');
	$add->(Symbol  => 'Symbol');
	$add->(Type    => 'Type');
	$add->(Integer => 'Int');
	$add->(Integer => 'IntU', min => 0);
	$add->(Array   => 'Matrix');
	$add->(Array   => 'Array', dim => [ undef ]);
	$add->(Record  => 'Record', extra_field_type => type_Any());
	$add->(Integer => 'Bit', min => 0, bits => 1);
	$add->(Integer => 'Byte', min => 0, bits => 8);
	$add->(Array   => 'ByteArray', elem_type => type_Byte(), dim => [ undef ]);
	$add->(Array   => 'String', elem_type => type_Byte(), dim => [ undef ], metadata => { charset => 'utf-8' } );
	$add->(Integer => 'Int16',  bits => 16, align => 1);
	$add->(Integer => 'Int16U', bits => 16, align => 1, min => 0);
	$add->(Integer => 'Int32',  bits => 32, align => 2);
	$add->(Integer => 'Int32U', bits => 32, align => 2, min => 0);
	$add->(Integer => 'Int64',  bits => 64, align => 3);
	$add->(Integer => 'Int64U', bits => 64, align => 3, min => 0);
}

our (
	$type_Typedef, $type_TypedefAny, $type_TypedefSymbol, $type_TypedefType,
	$type_TypedefInteger, $type_TypedefChoice, $type_TypedefArray, $type_TypedefRecord
);

sub type_Typedef {
	$type_Typedef ||= Userp::Type::Choice->new(
		options => [
			{ type => type_TypedefAny(),     merge_count => 1<<1 },
			{ type => type_TypedefSymbol(),  merge_count => 1<<1 },
			{ type => type_TypedefType(),    merge_count => 1<<1 },
			{ type => type_TypedefInteger(), merge_count => 1<<6 },
			{ type => type_TypedefChoice(),  merge_count => 1<<3 },
			{ type => type_TypedefArray(),   merge_count => 1<<6 },
			{ type => type_TypedefRecord(),  merge_count => 1<<6 },
		]
	);
}

sub type_TypedefInteger {
	$type_TypedefInteger ||= do {
		my $nametable_t= Userp::Type::Array->new(
			elem_type => Userp::Type::Choice->new(
				options => [
					Userp::Type::Record->new(fields => [
						[ value => type_Int ],
						[ name => type_Symbol ],
					]),
					{ type => type_Symbol, merge_ofs => 1 },
				]
			),
			dims => [undef],
		);
		Userp::Type::Record->new(
			fields => [
				[ name   => type_Symbol,  ALWAYS ],
				[ parent => type_Type,    OFTEN ],
				[ align  => type_Int,     OFTEN ],
				[ min    => type_Int,     OFTEN ],
				[ max    => type_Int,     OFTEN ],
				[ bits   => type_Int,     OFTEN ],
				[ names  => $nametable_t, OFTEN ],
			]
		);
	};
}

sub type_TypedefChoice {
	$type_TypedefChoice ||= do {
		my $option_t= Userp::Type::Choice->new(
			options => [
				{
					type => Userp::Type::Record->new(
						fields => [
							[ type        => type_Type, ALWAYS ],
							[ merge_ofs   => type_Int,  OFTEN ],
							[ merge_count => type_Int,  OFTEN ],
						]
					),
					merge_ofs => 0,
					merge_count => 4
				},
				{
					type => type_Any,
					merge_ofs => 0,
				}
			]
		);
		my $optarray_t= Userp::Type::Array->new(
			elem_type => $option_t,
			dims => [undef]
		);
		Userp::Type::Record->new(
			fields => [
				[ name    => type_Symbol, OFTEN ],
				[ parent  => type_Type,   OFTEN ],
				[ options => $optarray_t, OFTEN ],
				# [ align ]
			]
		);
	}
}

sub type_TypedefArray {
	$type_TypedefArray ||= do {
		my $intarray_t= Userp::Type::Array->new(
			elem_type => type_Int,
			dims => [ undef ]
		);
		Userp::Type::Record->new(
			fields => [
				[ name      => type_Symbol, OFTEN ],
				[ parent    => type_Type,   OFTEN ],
				[ elem_type => type_Type,   OFTEN ],
				[ dim_type  => type_Type,   OFTEN ],
				[ dims      => $intarray_t, OFTEN ],
				[ align     => type_Int,    OFTEN ],
			],
		);
	}
}

sub type_TypedefRecord {
	$type_TypedefRecord ||= do {
		my $placement_t= Userp::Type::Integer->new(
			min => SELDOM-1, # allow one value for "inherit"
			names => {
				'SELDOM' => SELDOM,
				'OFTEN'  => OFTEN,
				'ALWAYS' => ALWAYS
			},
		);
		my $field_t= Userp::Type::Record->new(
			fields => [
				[ name      => type_Symbol,  ALWAYS ],
				[ type      => type_Type,    ALWAYS ],
				[ placement => $placement_t, ALWAYS ],
			],
		);
		my $fieldarray_t= Userp::Type::Array->new(
			elem_type => $field_t,
			dims => [undef]
		);
		Userp::Type::Record->new(
			fields => [
				[ name             => type_Symbol,   OFTEN ],
				[ parent           => type_Type,     OFTEN ],
				[ fields           => $fieldarray_t, OFTEN ],
				[ extra_field_type => type_Type,     OFTEN ],
				[ static_bits      => type_Int,      OFTEN ],
				[ align            => type_Int,      OFTEN ],
			],
		);
	};
}

1;
